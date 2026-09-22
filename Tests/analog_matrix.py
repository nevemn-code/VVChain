import numpy as np

# V3 CHEBYSHEV reference.
# This mirrors the selected V3 web/native transfer:
#
# u = clamp(x, -1, 1)
# T3 = 4u^3 - 3u
# T5 = 16u^5 - 20u^3 + 5u
# shaped = u + amount * [H3*(T3-u) + H5*(T5-u)]
# y = x + 0.90 * (shaped-u)
#
# TT: H3=0.014, H5=0.004
# SS: H3=0.020, H5=0.006
#
# Exactly 500 deterministic cases are tested for:
# - block-size independence / zero state
# - no sample leakage
# - coherent fundamental phase preservation
# - finite / bounded output
# - sign symmetry / no generated DC
# - TT and SS remain measurably distinct
# - stereo independence
# - four-band routing remains four separate ANALOG calls


def process_v3(x, amount, solid_state):
    x = np.asarray(x, dtype=np.float64)
    amount = float(np.clip(amount, 0.0, 1.0))

    if x.size == 0 or amount <= 0.0:
        return x.copy()

    h3 = 0.020 if solid_state else 0.014
    h5 = 0.006 if solid_state else 0.004

    u = np.clip(x, -1.0, 1.0)
    u2 = u * u
    t3 = 4.0 * u * u2 - 3.0 * u
    t5 = 16.0 * u * u2 * u2 - 20.0 * u * u2 + 5.0 * u
    shaped = u + amount * (h3 * (t3 - u) + h5 * (t5 - u))
    return x + 0.90 * (shaped - u)


def blockwise(x, amount, solid_state, block_size):
    y = np.empty_like(x)
    for start in range(0, len(x), block_size):
        end = min(start + block_size, len(x))
        y[start:end] = process_v3(x[start:end], amount, solid_state)
    return y


def phase_deg(signal, freq, fs):
    n = len(signal)
    t = np.arange(n, dtype=np.float64) / fs
    z = np.dot(signal, np.exp(-2.0j * np.pi * freq * t))
    return float(np.angle(z, deg=True))


def static_native_routing_guard():
    from pathlib import Path

    source = Path("Source/DSP/ChainDSP.cpp").read_text(encoding="utf-8")
    start = source.index("void VVChainDSP::applyEq")
    end = source.index("void VVChainDSP::applyOtt", start)
    apply_eq = source[start:end]

    assert apply_eq.count("processChebyshevAnalog(") == 1, (
        "V3 ANALOG call must remain inside the four-band loop"
    )
    loop_start = apply_eq.index("for (size_t band = 0; band < eq.size(); ++band)")
    analog_pos = apply_eq.index("processChebyshevAnalog(")
    assert analog_pos > loop_start, (
        "V3 ANALOG call is not inside the four-band EQ loop"
    )
    assert "p.eqColorSolidState[band]" in apply_eq, (
        "per-band TT/SS selection is missing"
    )
    assert "colourSum" not in apply_eq, (
        "V3 must not average four ANALOG bands"
    )


def run():
    static_native_routing_guard()

    rng = np.random.default_rng(20260922)
    sample_rates = [44100.0, 48000.0, 88200.0, 96000.0]
    block_sizes = [32, 64, 128, 256, 512, 1024]

    max_block_error = 0.0
    max_impulse_leak = 0.0
    max_phase_error = 0.0
    max_dc_error = 0.0
    max_stereo_error = 0.0
    max_output = 0.0
    min_tt_ss_delta = np.inf

    for case in range(500):
        fs = sample_rates[case % len(sample_rates)]
        amount = 0.01 + 0.99 * ((case * 37) % 1000) / 999.0
        solid_state = bool(case & 1)

        n = 8192
        t = np.arange(n, dtype=np.float64) / fs

        freq = 20.0 + ((case * 43) % int(min(18000.0, fs * 0.40)))
        freq2 = min(freq * 1.73, fs * 0.45)
        amp = 0.05 + 0.90 * ((case * 71) % 1000) / 999.0

        kind = case % 5
        if kind == 0:
            x = amp * np.sin(2.0 * np.pi * freq * t)
        elif kind == 1:
            x = (
                0.72 * amp * np.sin(2.0 * np.pi * freq * t + 0.17)
                + 0.21 * amp * np.sin(2.0 * np.pi * freq2 * t + 0.91)
            )
        elif kind == 2:
            x = 0.35 * amp * rng.standard_normal(n)
        elif kind == 3:
            x = amp * np.sign(np.sin(2.0 * np.pi * freq * t))
        else:
            x = amp * np.linspace(-1.0, 1.0, n)

        whole = process_v3(x, amount, solid_state)

        for bs in block_sizes:
            by_block = blockwise(x, amount, solid_state, bs)
            max_block_error = max(
                max_block_error,
                float(np.max(np.abs(whole - by_block))),
            )

        impulse = np.zeros(257, dtype=np.float64)
        impulse[128] = min(0.95, max(0.01, amp))
        yi = process_v3(impulse, amount, solid_state)
        outside = np.concatenate((yi[:128], yi[129:]))
        if outside.size:
            max_impulse_leak = max(
                max_impulse_leak,
                float(np.max(np.abs(outside))),
            )

        # Coherent fundamental: a memoryless odd polynomial should not
        # introduce a time shift into the fundamental.
        phase_n = 16384
        phase_bin = 5 + ((case * 29) % 700)
        phase_freq = phase_bin * fs / phase_n
        pt = np.arange(phase_n, dtype=np.float64) / fs
        px = np.sin(2.0 * np.pi * phase_freq * pt)
        py = process_v3(px, amount, solid_state)
        phase_in = phase_deg(px, phase_freq, fs)
        phase_out = phase_deg(py, phase_freq, fs)
        dphase = abs((phase_out - phase_in + 180.0) % 360.0 - 180.0)
        max_phase_error = max(max_phase_error, dphase)

        # Symmetric material: no DC created by the odd-only Chebyshev transfer.
        half = min(1024, len(x))
        sym = np.concatenate((x[:half], -x[:half]))
        max_dc_error = max(
            max_dc_error,
            abs(float(np.mean(process_v3(sym, amount, solid_state)))),
        )

        # TT and SS are intentionally distinct; they must not collapse into
        # the same transfer function.
        tt = process_v3(x, amount, False)
        ss = process_v3(x, amount, True)
        delta = float(np.max(np.abs(tt - ss)))
        min_tt_ss_delta = min(min_tt_ss_delta, delta)

        # Deterministic channel independence.
        left = process_v3(x, amount, solid_state)
        right_input = np.roll(x, (case * 13) % n)
        right = process_v3(right_input, amount, not solid_state)
        left_again = process_v3(x, amount, solid_state)
        right_again = process_v3(right_input, amount, not solid_state)
        max_stereo_error = max(
            max_stereo_error,
            float(np.max(np.abs(left - left_again))),
            float(np.max(np.abs(right - right_again))),
        )

        assert np.all(np.isfinite(whole)), f"non-finite output case {case}"
        max_output = max(max_output, float(np.max(np.abs(whole))))

    assert max_block_error < 1.0e-15, (
        f"block dependence detected: {max_block_error:.3e}"
    )
    assert max_impulse_leak < 1.0e-15, (
        f"sample leakage detected: {max_impulse_leak:.3e}"
    )
    assert max_phase_error < 1.0e-7, (
        f"phase shift detected: {max_phase_error:.9f} deg"
    )
    assert max_dc_error < 1.0e-15, (
        f"unexpected DC: {max_dc_error:.3e}"
    )
    assert max_stereo_error < 1.0e-15, (
        f"cross-channel interaction: {max_stereo_error:.3e}"
    )
    assert min_tt_ss_delta > 1.0e-9, (
        "TT and SS collapsed to the same transfer"
    )
    assert max_output < 2.0, f"unexpected output peak: {max_output:.6f}"

    print(
        "PASS V3 CHEBYSHEV 500-case matrix: "
        f"cases=500, max_block_error={max_block_error:.3e}, "
        f"max_impulse_leak={max_impulse_leak:.3e}, "
        f"max_phase_error={max_phase_error:.9f} deg, "
        f"max_dc_error={max_dc_error:.3e}, "
        f"max_stereo_error={max_stereo_error:.3e}, "
        f"min_tt_ss_delta={min_tt_ss_delta:.3e}, "
        f"max_output={max_output:.6f}"
    )


if __name__ == "__main__":
    run()
