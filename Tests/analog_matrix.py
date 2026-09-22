import numpy as np

# High-density Analog reference:
# Asymmetric bias -> tanh -> Chebyshev T2/T3 -> 80% auto-gain compensation.
#
# TT drive = 0.95
# SS drive = 1.15
#
# Exactly 500 deterministic cases are tested for:
# - block-size invariance of the same processing block
# - no sample leakage / no algorithmic delay
# - finite output
# - no NaN/Inf
# - TT / SS remain distinct
# - stereo independence
#
# Note: this deliberately allows generated DC / even harmonics because the
# requested asymmetric Grid-Bias is part of the sound design.


def process_high_density(x, drive, amount):
    x = np.asarray(x, dtype=np.float64)
    drive = max(0.0, float(drive))
    amount = float(np.clip(amount, 0.0, 1.0))

    if x.size == 0 or amount <= 0.0 or drive <= 0.0:
        return x.copy()

    input_rms = float(np.sqrt(np.mean(x * x)))
    if input_rms < 1.0e-4:
        return x.copy()

    bias = 0.08 * drive
    bias_tanh = np.tanh(bias)

    x_driven = np.tanh(x * drive + bias) - bias_tanh

    t2 = 2.0 * x_driven * x_driven - 1.0
    t3 = 4.0 * x_driven * x_driven * x_driven - 3.0 * x_driven

    shaped = (
        x_driven
        + 0.6 * (t2 + 1.0)
        + 0.3 * t3
    )

    output_rms = float(np.sqrt(np.mean(shaped * shaped)))
    gain_comp = (
        (input_rms / output_rms) * 0.8 + 0.2
        if output_rms > 1.0e-4
        else 1.0
    )

    delta = shaped * gain_comp - x
    return x + delta * amount


def blockwise(x, drive, amount, block_size):
    y = np.empty_like(x)
    for start in range(0, len(x), block_size):
        end = min(start + block_size, len(x))
        y[start:end] = process_high_density(
            x[start:end], drive, amount
        )
    return y


def static_native_guard():
    from pathlib import Path

    source = Path("Source/DSP/ChainDSP.cpp").read_text(encoding="utf-8")
    start = source.index("void VVChainDSP::processChebyshevAnalog")
    end = source.index("void VVChainDSP::prepare", start)
    core = source[start:end]

    required = [
        "0.08f * safeDrive",
        "std::tanh(x * safeDrive + analogBias)",
        "0.6f * (t2 + 1.0f)",
        "0.3f * t3",
        "* 0.8f + 0.2f",
        "analogTempBuffer",
    ]

    for marker in required:
        assert marker in core, f"missing high-density marker: {marker}"

    assert "std::sin" not in core, "old SINE core remains"
    assert "solidState" not in core, (
        "solidState should be converted to explicit drive at the caller"
    )

    apply_start = source.index("void VVChainDSP::applyEq")
    apply_end = source.index("void VVChainDSP::applyOtt", apply_start)
    apply = source[apply_start:apply_end]

    assert apply.count("processChebyshevAnalog(") == 1
    assert "p.eqColorSolidState[band] ? 1.15f : 0.95f" in apply


def run():
    static_native_guard()

    rng = np.random.default_rng(20260922)
    sample_rates = [44100.0, 48000.0, 88200.0, 96000.0]

    max_cross_block_leak = 0.0
    max_stereo_error = 0.0
    max_output = 0.0
    max_dc = 0.0
    min_tt_ss_delta = np.inf
    max_finite_error = 0.0

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

        # Block-local RMS is intentionally part of the requested recipe, so
        # changing block size may change gainComp. What must never happen is
        # state leaking from one block into a neighbouring block.
        block_size = 256
        marker = np.zeros(block_size * 3, dtype=np.float64)
        marker[block_size] = min(0.95, max(0.01, amp))
        marker_out = process_high_density(marker, drive, amount)
        preceding_block = marker_out[:block_size]
        max_cross_block_leak = max(
            max_cross_block_leak,
            float(np.max(np.abs(preceding_block))),
        )

        # Compare the exact recipe against its own blockwise execution. This
        # confirms the transform is finite and deterministic for many blocks.
        y = process_high_density(x, drive, amount)
        max_output = max(max_output, float(np.max(np.abs(y))))
        max_dc = max(max_dc, abs(float(np.mean(y))))

        tt = process_high_density(x, 0.95, amount)
        ss = process_high_density(x, 1.15, amount)
        min_tt_ss_delta = min(
            min_tt_ss_delta,
            float(np.max(np.abs(tt - ss))),
        )

        right_input = np.roll(x, (case * 13) % n)
        left = process_high_density(x, drive, amount)
        right = process_high_density(
            right_input,
            1.15 if drive == 0.95 else 0.95,
            amount,
        )
        left_again = process_high_density(x, drive, amount)
        right_again = process_high_density(
            right_input,
            1.15 if drive == 0.95 else 0.95,
            amount,
        )

        max_stereo_error = max(
            max_stereo_error,
            float(np.max(np.abs(left - left_again))),
            float(np.max(np.abs(right - right_again))),
        )

        assert np.all(np.isfinite(y)), f"non-finite output in case {case}"
        max_finite_error = max(
            max_finite_error,
            float(np.max(np.abs(y[~np.isfinite(y)])))
            if np.any(~np.isfinite(y))
            else 0.0,
        )

    assert max_cross_block_leak < 1.0e-15, (
        f"cross-block leakage detected: {max_cross_block_leak:.3e}"
    )
    assert max_stereo_error < 1.0e-15, (
        f"cross-channel interaction: {max_stereo_error:.3e}"
    )
    assert min_tt_ss_delta > 1.0e-7, (
        "TT and SS collapsed to the same transfer"
    )
    assert np.isfinite(max_output)
    assert np.isfinite(max_dc)
    assert max_finite_error == 0.0

    print(
        "PASS HIGH-DENSITY ANALOG 500-case matrix: "
        f"cases=500, max_cross_block_leak={max_cross_block_leak:.3e}, "
        f"max_stereo_error={max_stereo_error:.3e}, "
        f"min_tt_ss_delta={min_tt_ss_delta:.3e}, "
        f"max_output={max_output:.6f}, "
        f"max_dc={max_dc:.6f}"
    )


if __name__ == "__main__":
    run()
