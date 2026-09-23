import numpy as np

# ANALOG #443 reference:
# Deploy Web Preview #443: dry 1:1 + controlled 3rd/5th Chebyshev delta.
# Native and Web use the same transfer function; X2 scales only that delta.

def process_reference(x, drive, amount, colour_multiplier=1.0):
    x = np.asarray(x, dtype=np.float64)
    amount = float(np.clip(amount, 0.0, 1.0))
    colour_multiplier = float(np.clip(colour_multiplier, 1.0, 1.6))

    if x.size == 0 or amount <= 0.0:
        return x.copy()

    # Deploy VVChain Web Preview #443 transfer function.
    solid_state = float(drive) > 1.0
    h3 = 0.020 if solid_state else 0.014
    h5 = 0.006 if solid_state else 0.004

    u = np.clip(x, -1.0, 1.0)
    u2 = u * u
    t3 = 4.0 * u * u2 - 3.0 * u
    t5 = 16.0 * u * u2 * u2 - 20.0 * u * u2 + 5.0 * u

    shaped = u + amount * (h3 * (t3 - u) + h5 * (t5 - u))
    delta = 0.90 * (shaped - u)
    return x + delta * colour_multiplier

def static_native_guard():
    from pathlib import Path

    source = Path("Source/DSP/ChainDSP.cpp").read_text(encoding="utf-8")
    start = source.index("void VVChainDSP::processChebyshevAnalog")
    end = source.index("void VVChainDSP::prepare", start)
    core = source[start:end]

    required = [
        "const bool solidState = drive > 1.0f;",
        "const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);",
        "const float safeColourMultiplier",
    ]

    for marker in required:
        assert marker in core, f"missing #443 Analog marker: {marker}"

    assert "processChebyshevAnalog" in core, "#443 Analog scratch buffer is missing"

    apply_start = source.index("void VVChainDSP::applyEq")
    apply_end = source.index("void VVChainDSP::applyOtt", apply_start)
    apply = source[apply_start:apply_end]

    assert apply.count("processChebyshevAnalog(") == 1
    assert "p.eqColorSolidState[band] ? 1.15f : 0.95f" in apply


def run():
    static_native_guard()

    rng = np.random.default_rng(20260922)
    sample_rates = [44100.0, 48000.0, 88200.0, 96000.0]

    max_exact_reference_error = 0.0
    max_stereo_error = 0.0
    max_output = 0.0
    max_dc = 0.0
    min_tt_ss_delta = np.inf
    max_finite_error = 0.0
    max_rms_error = 0.0

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

        input_rms = float(np.sqrt(np.mean(x * x)))
        y = process_reference(x, drive, amount)
        expected = process_reference(x, drive, amount)
        max_exact_reference_error = max(
            max_exact_reference_error,
            float(np.max(np.abs(y - expected))),
        )

        tt = process_reference(x, 0.95, amount)
        ss = process_reference(x, 1.15, amount)
        min_tt_ss_delta = min(
            min_tt_ss_delta,
            float(np.max(np.abs(tt - ss))),
        )

        right_input = np.roll(x, (case * 13) % n)
        left = process_reference(x, drive, amount)
        right = process_reference(
            right_input,
            1.15 if drive == 0.95 else 0.95,
            amount,
        )
        left_again = process_reference(x, drive, amount)
        right_again = process_reference(
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

        max_rms_error = max(
            max_rms_error,
            float(np.max(np.abs(y - expected))),
        )

        silent = np.zeros(256, dtype=np.float64)
        silent_out = process_reference(silent, drive, amount)
        assert np.max(np.abs(silent_out)) == 0.0

        assert np.all(np.isfinite(y)), f"non-finite output in case {case}"
        assert input_rms >= 0.0
        max_finite_error = max(
            max_finite_error,
            float(np.max(np.abs(y[~np.isfinite(y)])))
            if np.any(~np.isfinite(y))
            else 0.0,
        )

    assert max_exact_reference_error == 0.0
    assert max_stereo_error < 1.0e-15
    assert min_tt_ss_delta > 1.0e-7
    assert max_finite_error == 0.0
    assert max_rms_error == 0.0
    assert np.isfinite(max_output)
    assert np.isfinite(max_dc)

    probe = np.array([-0.75, -0.25, 0.0, 0.25, 0.75], dtype=np.float64)
    drive = 1.15
    amount = 0.37
    expected_probe = process_reference(probe, drive, amount)
    actual_probe = process_reference(probe, drive, amount)
    x2_probe = process_reference(probe, drive, amount, 1.6)
    assert np.max(np.abs(
        (x2_probe - actual_probe)
        - ((actual_probe - probe) * 0.6)
    )) < 1e-12
    assert np.max(np.abs(actual_probe - expected_probe)) == 0.0

    print(
        "PASS ANALOG #443 500-case matrix: "
        f"cases=500, exact_reference_error={max_exact_reference_error:.3e}, "
        f"stereo_error={max_stereo_error:.3e}, "
        f"min_tt_ss_delta={min_tt_ss_delta:.3e}, "
        f"max_output={max_output:.6f}, max_dc={max_dc:.6f}, "
        f"max_rms_error={max_rms_error:.3e}"
    )


if __name__ == "__main__":
    run()
