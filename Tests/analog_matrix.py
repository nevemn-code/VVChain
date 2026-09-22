import numpy as np

# Final Analog reference:
# tanh soft clip -> T2 + 0.15 * (4*x^3) harmonic colour ->
# 100% RMS match on the coloured branch -> serial Delta mix.
#
# TT drive = 0.95
# SS drive = 1.15
#
# The final serial output is:
#     y = x + amount * (matched_colour - x)
# Therefore exact final-output RMS equality is guaranteed only at amount=1.0;
# the matched colour branch itself is always RMS matched to the input block.


def process_final(x, drive, amount):
    x = np.asarray(x, dtype=np.float64)
    drive = max(0.0, float(drive))
    amount = float(np.clip(amount, 0.0, 1.0))

    if x.size == 0 or amount <= 0.0 or drive <= 0.0:
        return x.copy(), x.copy(), 1.0

    input_rms = float(np.sqrt(np.mean(x * x)))
    if input_rms < 1.0e-4:
        return x.copy(), x.copy(), 1.0

    x_driven = np.tanh(x * drive)
    even_harmonics = 2.0 * x_driven * x_driven - 1.0
    odd_harmonics = 4.0 * x_driven * x_driven * x_driven

    shaped = (
        x_driven
        + 0.25 * even_harmonics
        + 0.15 * odd_harmonics
    )

    output_rms = float(np.sqrt(np.mean(shaped * shaped)))
    gain_comp = input_rms / output_rms if output_rms > 1.0e-4 else 1.0

    matched = shaped * gain_comp
    delta = matched - x
    y = x + delta * amount

    return y, matched, gain_comp


def static_native_guard():
    from pathlib import Path

    source = Path("Source/DSP/ChainDSP.cpp").read_text(encoding="utf-8")
    start = source.index("void VVChainDSP::processChebyshevAnalog")
    end = source.index("void VVChainDSP::prepare", start)
    core = source[start:end]

    required = [
        "std::tanh(x * safeDrive)",
        "2.0f * xDriven * xDriven - 1.0f",
        "4.0f * xDriven * xDriven * xDriven",
        "0.25f * evenHarmonics",
        "0.15f * oddHarmonics",
        "inputRms / outputRms",
    ]

    for marker in required:
        assert marker in core, f"missing final Analog marker: {marker}"

    assert "0.8f" not in core, "old 80% auto-gain remains"
    assert "0.6f" not in core, "old high-density coefficient remains"
    assert "analogBias" not in core, "old asymmetric bias remains"

    apply_start = source.index("void VVChainDSP::applyEq")
    apply_end = source.index("void VVChainDSP::applyOtt", apply_start)
    apply = source[apply_start:apply_end]

    assert apply.count("processChebyshevAnalog(") == 1
    assert "p.eqColorSolidState[band] ? 1.15f : 0.95f" in apply


def run():
    static_native_guard()

    rng = np.random.default_rng(20260922)
    sample_rates = [44100.0, 48000.0, 88200.0, 96000.0]

    max_preceding_block_leak = 0.0
    max_stereo_error = 0.0
    max_branch_rms_error = 0.0
    max_output = 0.0
    max_dc = 0.0
    min_tt_ss_delta = np.inf

    for case in range(500):
        fs = sample_rates[case % len(sample_rates)]
        amount = 0.01 + 0.99 * ((case * 37) % 1000) / 999.0
        drive = 0.95 if (case & 1) == 0 else 1.15

        n = 8192
        t = np.arange(n, dtype=np.float64) / fs
        freq = 20.0 + ((case * 43) % int(min(18000, fs * 0.40)))
        amp = 0.02 + 0.95 * ((case * 71) % 1000) / 999.0

        kind = case % 5
        if kind == 0:
            x = amp * np.sin(2.0 * np.pi * freq * t)
        elif kind == 1:
            f2 = min(freq * 1.73, fs * 0.45)
            x = (
                0.72 * amp * np.sin(2.0 * np.pi * freq * t + 0.17)
                + 0.21 * amp * np.sin(2.0 * np.pi * f2 * t + 0.91)
            )
        elif kind == 2:
            x = 0.35 * amp * rng.standard_normal(n)
        elif kind == 3:
            x = amp * np.sign(np.sin(2.0 * np.pi * freq * t))
        else:
            x = amp * np.linspace(-1.0, 1.0, n)

        y, matched, gain_comp = process_final(x, drive, amount)

        input_rms = float(np.sqrt(np.mean(x * x)))
        matched_rms = float(np.sqrt(np.mean(matched * matched)))
        if input_rms > 1.0e-6:
            max_branch_rms_error = max(
                max_branch_rms_error,
                abs(matched_rms / input_rms - 1.0),
            )

        # Memoryless/no-delay guard: a one-sample marker cannot appear in the
        # immediately preceding block.
        block_size = 256
        marker = np.zeros(block_size * 3, dtype=np.float64)
        marker[block_size] = min(0.95, max(0.01, amp))
        marker_out, _, _ = process_final(marker, drive, amount)
        max_preceding_block_leak = max(
            max_preceding_block_leak,
            float(np.max(np.abs(marker_out[:block_size]))),
        )

        tt, _, _ = process_final(x, 0.95, amount)
        ss, _, _ = process_final(x, 1.15, amount)
        min_tt_ss_delta = min(
            min_tt_ss_delta,
            float(np.max(np.abs(tt - ss))),
        )

        right_input = np.roll(x, (case * 13) % n)
        left, _, _ = process_final(x, drive, amount)
        right, _, _ = process_final(
            right_input,
            1.15 if drive == 0.95 else 0.95,
            amount,
        )
        left_again, _, _ = process_final(x, drive, amount)
        right_again, _, _ = process_final(
            right_input,
            1.15 if drive == 0.95 else 0.95,
            amount,
        )

        max_stereo_error = max(
            max_stereo_error,
            float(np.max(np.abs(left - left_again))),
            float(np.max(np.abs(right - right_again))),
        )

        max_output = max(max_output, float(np.max(np.abs(y))))
        max_dc = max(max_dc, abs(float(np.mean(y))))

        assert np.all(np.isfinite(y)), f"non-finite output in case {case}"
        assert np.isfinite(gain_comp), f"non-finite gainComp in case {case}"

    assert max_preceding_block_leak < 1.0e-15, (
        f"preceding-block leakage detected: {max_preceding_block_leak:.3e}"
    )
    assert max_stereo_error < 1.0e-15, (
        f"cross-channel interaction detected: {max_stereo_error:.3e}"
    )
    assert max_branch_rms_error < 1.0e-12, (
        f"100% matched-branch RMS error too large: {max_branch_rms_error:.3e}"
    )
    assert min_tt_ss_delta > 1.0e-7, (
        "TT and SS collapsed to the same transfer"
    )

    # At 100% amount the final serial output is exactly the RMS-matched branch.
    probe = np.sin(2.0 * np.pi * 997.0 * np.arange(8192) / 48000.0)
    final_100, matched_100, _ = process_final(probe, 0.95, 1.0)
    assert np.max(np.abs(final_100 - matched_100)) < 1.0e-15

    assert np.isfinite(max_output)
    assert np.isfinite(max_dc)

    print(
        "PASS FINAL ANALOG 500-case matrix: "
        f"cases=500, preceding_block_leak={max_preceding_block_leak:.3e}, "
        f"branch_rms_error={max_branch_rms_error:.3e}, "
        f"stereo_error={max_stereo_error:.3e}, "
        f"min_tt_ss_delta={min_tt_ss_delta:.3e}, "
        f"max_output={max_output:.6f}, "
        f"max_dc={max_dc:.6f}"
    )


if __name__ == "__main__":
    run()
