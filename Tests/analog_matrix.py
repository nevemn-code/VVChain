import numpy as np

# ANALOG v1.0.15 reference:
# smooth odd-symmetric algebraic saturation with |x|=1 unity normalization.
# 0% is exact dry; X2 still multiplies only the generated ANALOG delta.

def process_reference(x, drive, amount, colour_multiplier=1.0):
    x = np.asarray(x, dtype=np.float64)
    amount = float(np.clip(amount, 0.0, 1.0))
    colour_multiplier = float(np.clip(colour_multiplier, 1.0, 1.6))

    if x.size == 0 or amount <= 0.0:
        return x.copy()

    solid_state = float(drive) > 1.0
    mode_alpha = 1.80 if solid_state else 1.55
    alpha = amount * mode_alpha
    unity_norm = (1.0 + alpha) ** 0.25

    u = np.clip(x, -1.0, 1.0)
    denominator = np.sqrt(np.sqrt(1.0 + alpha * u * u))
    saturated = (u / denominator) * unity_norm
    return x + (saturated - u) * colour_multiplier


def static_native_guard():
    from pathlib import Path

    source = Path("Source/DSP/ChainDSP.cpp").read_text(encoding="utf-8")
    start = source.index("void VVChainDSP::processChebyshevAnalog")
    end = source.index("void VVChainDSP::prepare", start)
    core = source[start:end]

    required = [
        "const bool solidState = drive > 1.0f;",
        "const double modeAlpha = solidState ? 1.80 : 1.55;",
        "const double unityNorm = std::pow(1.0 + alpha, 0.25);",
        "const double u = juce::jlimit(-1.0, 1.0, x);",
        "x + (saturated - u)",
        "safeColourMultiplier",
    ]

    for marker in required:
        assert marker in core, f"missing v1.0.15 Analog marker: {marker}"

    apply_start = source.index("void VVChainDSP::applyEq")
    apply_end = source.index("void VVChainDSP::applyOtt", apply_start)
    apply = source[apply_start:apply_end]
    assert apply.count("processChebyshevAnalog(") == 1
    assert "p.eqColorSolidState[band] ? 1.15f : 0.95f" in apply


def run():
    static_native_guard()

    rng = np.random.default_rng(20260924)
    sample_rates = [44100.0, 48000.0, 88200.0, 96000.0]

    max_stereo_error = 0.0
    max_output = 0.0
    max_dc = 0.0
    min_tt_ss_delta = np.inf
    max_finite_error = 0.0
    max_shrink = 0.0

    for case in range(500):
        fs = sample_rates[case % len(sample_rates)]
        amount = ((case * 37) % 1001) / 1000.0
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

        y = process_reference(x, drive, amount)
        assert np.all(np.isfinite(y)), f"non-finite output in case {case}"

        # 0% must be bit-transparent.
        if amount == 0.0:
            assert np.array_equal(y, x)

        # In the documented -1..+1 input domain, ANALOG must never shrink
        # sample magnitude as the control is increased.
        mask = np.abs(x) <= 1.0
        if np.any(mask):
            shrink = np.abs(x[mask]) - np.abs(y[mask])
            max_shrink = max(max_shrink, float(np.max(shrink)))
            assert np.max(shrink) <= 1.0e-12

        # Stereo/channel determinism.
        right_input = np.roll(x, (case * 13) % n)
        left = process_reference(x, drive, amount)
        right = process_reference(right_input, drive, amount)
        left_again = process_reference(x, drive, amount)
        right_again = process_reference(right_input, drive, amount)
        max_stereo_error = max(
            max_stereo_error,
            float(np.max(np.abs(left-left_again))),
            float(np.max(np.abs(right-right_again))),
        )

        tt = process_reference(x, 0.95, max(amount, 0.01))
        ss = process_reference(x, 1.15, max(amount, 0.01))
        min_tt_ss_delta = min(
            min_tt_ss_delta,
            float(np.max(np.abs(tt - ss))),
        )

        max_output = max(max_output, float(np.max(np.abs(y))))
        max_dc = max(max_dc, abs(float(np.mean(y))))
        max_finite_error = max(
            max_finite_error,
            float(np.max(np.abs(y[~np.isfinite(y)])))
            if np.any(~np.isfinite(y)) else 0.0,
        )

    # Boundary guarantees.
    for drive in (0.95, 1.15):
        for amount in np.linspace(0.0, 1.0, 51):
            edge = np.array([-1.0, 0.0, 1.0])
            out = process_reference(edge, drive, amount)
            assert np.max(np.abs(out-edge)) < 1.0e-12

    # X2 still multiplies only the generated delta.
    probe = np.array([-0.75, -0.25, 0.0, 0.25, 0.75], dtype=np.float64)
    base = process_reference(probe, 1.15, 0.37, 1.0)
    x2 = process_reference(probe, 1.15, 0.37, 1.6)
    assert np.max(np.abs((x2-probe) - (base-probe)*1.6)) < 1e-12

    # Odd symmetry => no algorithmic DC bias.
    odd_probe = np.linspace(-1.0, 1.0, 10001)
    odd_out = process_reference(odd_probe, 1.15, 1.0)
    assert np.max(np.abs(odd_out + odd_out[::-1])) < 1e-12

    assert max_stereo_error < 1.0e-15
    assert min_tt_ss_delta > 1.0e-7
    assert max_finite_error == 0.0
    assert max_shrink <= 1.0e-12
    assert np.isfinite(max_output)
    assert np.isfinite(max_dc)

    print(
        "PASS ANALOG v1.0.15 500-case matrix: "
        f"cases=500, stereo_error={max_stereo_error:.3e}, "
        f"min_tt_ss_delta={min_tt_ss_delta:.3e}, "
        f"max_output={max_output:.6f}, max_dc={max_dc:.6f}, "
        f"max_shrink={max_shrink:.3e}"
    )


if __name__ == "__main__":
    run()
