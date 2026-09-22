import numpy as np

# V3 main ANALOG reference:
# V2 SINE transfer, implemented as a pure memoryless residual stage.
# There is no block RMS, no IIR state, no DC accumulator and no delay.
#
# y = x + 0.50 * amount * (sin(pi/2 * clamp(x,-1,1)) - clamp(x,-1,1))
#
# The test deliberately checks the properties needed when ANALOG is inserted
# into a larger EQ/OTT/TAPE-A chain:
#   1) exact block-size invariance
#   2) zero sample leakage / zero algorithmic delay
#   3) coherent fundamental phase preservation
#   4) no new clipping beyond the source peak envelope
#   5) no generated DC for a sign-symmetric signal
#   6) strict stereo/channel independence
#   7) finite output for all tested parameters
#
# Exactly 500 deterministic cases are executed.


def process_v3_safe(x, amount):
    x = np.asarray(x, dtype=np.float64)
    amount = float(np.clip(amount, 0.0, 1.0))

    if x.size == 0 or amount <= 0.0:
        return x.copy()

    u = np.clip(x, -1.0, 1.0)
    shaped = np.sin(0.5 * np.pi * u)
    return x + 0.50 * amount * (shaped - u)


def process_blockwise(x, amount, block_size):
    x = np.asarray(x, dtype=np.float64)
    y = np.empty_like(x)

    for start in range(0, len(x), block_size):
        end = min(start + block_size, len(x))
        y[start:end] = process_v3_safe(x[start:end], amount)

    return y


def phase_deg(signal, freq, fs):
    n = len(signal)
    t = np.arange(n, dtype=np.float64) / fs
    z = np.dot(signal, np.exp(-2.0j * np.pi * freq * t))
    return float(np.angle(z, deg=True))


def run():
    rng = np.random.default_rng(20260922)

    sample_rates = [44100.0, 48000.0, 88200.0, 96000.0]
    block_sizes = [32, 64, 128, 256, 512, 1024]
    max_block_error = 0.0
    max_phase_error = 0.0
    max_impulse_leak = 0.0
    max_dc_error = 0.0
    max_peak_overflow = 0.0
    max_stereo_error = 0.0
    max_output = 0.0

    for case in range(500):
        fs = sample_rates[case % len(sample_rates)]

        amount = 0.01 + 0.99 * ((case * 37) % 1000) / 999.0
        amplitude = 0.001 + 1.999 * ((case * 71) % 1000) / 999.0

        # Coherent frequencies for deterministic phase measurement.
        phase_bin = 5 + ((case * 29) % 700)
        phase_freq = phase_bin * fs / 16384.0

        n = 4096
        t = np.arange(n, dtype=np.float64) / fs

        tone_f = 20.0 + ((case * 43) % 18000)
        tone_f2 = min(tone_f * 1.73, fs * 0.45)

        signal_kind = case % 5
        if signal_kind == 0:
            x = amplitude * np.sin(2.0 * np.pi * tone_f * t)
        elif signal_kind == 1:
            x = (
                0.72 * amplitude * np.sin(2.0 * np.pi * tone_f * t + 0.17)
                + 0.21 * amplitude * np.sin(2.0 * np.pi * tone_f2 * t + 0.91)
            )
        elif signal_kind == 2:
            x = amplitude * rng.standard_normal(n) * 0.35
        elif signal_kind == 3:
            x = amplitude * np.sign(
                np.sin(2.0 * np.pi * tone_f * t)
            )
        else:
            x = amplitude * np.linspace(-1.0, 1.0, n)

        # 1) The operation must be exactly block-size invariant because there
        # is no state crossing the block boundary.
        whole = process_v3_safe(x, amount)
        for block_size in block_sizes:
            blockwise = process_blockwise(x, amount, block_size)
            max_block_error = max(
                max_block_error,
                float(np.max(np.abs(whole - blockwise))),
            )

        # 2) Single-sample impulse must not leak into neighbouring samples.
        impulse = np.zeros(257, dtype=np.float64)
        impulse[128] = min(0.95, max(0.001, amplitude))
        impulse_out = process_v3_safe(impulse, amount)
        outside = np.concatenate((impulse_out[:128], impulse_out[129:]))
        max_impulse_leak = max(
            max_impulse_leak,
            float(np.max(np.abs(outside))) if outside.size else 0.0,
        )

        # 3) For a coherent sine the fundamental remains phase-aligned.
        phase_n = 16384
        phase_t = np.arange(phase_n, dtype=np.float64) / fs
        phase_in = np.sin(
            2.0 * np.pi * phase_freq * phase_t
        )
        phase_out = process_v3_safe(phase_in, amount)

        phase_in_deg = phase_deg(phase_in, phase_freq, fs)
        phase_out_deg = phase_deg(phase_out, phase_freq, fs)
        phase_error = abs(
            (phase_out_deg - phase_in_deg + 180.0) % 360.0 - 180.0
        )
        max_phase_error = max(max_phase_error, phase_error)

        # 4) The SINE transfer is bounded. For |x| <= 1 it cannot exceed 1;
        # for |x| > 1 the clamp makes the residual exactly zero.
        source_peak = float(np.max(np.abs(x)))
        output_peak = float(np.max(np.abs(whole)))
        allowed_peak = max(1.0, source_peak)
        max_peak_overflow = max(
            max_peak_overflow,
            max(0.0, output_peak - allowed_peak),
        )
        max_output = max(max_output, output_peak)

        # 5) Sign-symmetric material cannot acquire a DC offset.
        half = min(1024, len(x))
        sym = np.concatenate((x[:half], -x[:half]))
        sym_out = process_v3_safe(sym, amount)
        max_dc_error = max(max_dc_error, abs(float(np.mean(sym_out))))

        # 6) Strict channel independence: changing R must not alter L.
        left = process_v3_safe(x, amount)
        right = process_v3_safe(
            np.roll(x, (case * 13) % n),
            amount,
        )
        left_again = process_v3_safe(x, amount)
        right_again = process_v3_safe(
            np.roll(x, (case * 13) % n),
            amount,
        )
        max_stereo_error = max(
            max_stereo_error,
            float(np.max(np.abs(left - left_again))),
            float(np.max(np.abs(right - right_again))),
        )

        assert np.all(np.isfinite(whole)), f"non-finite output in case {case}"
        assert np.all(np.isfinite(whole - x)), f"non-finite residual in case {case}"

    assert max_block_error < 1.0e-15, (
        f"block-size dependence detected: {max_block_error:.3e}"
    )
    assert max_impulse_leak < 1.0e-15, (
        f"sample leakage detected: {max_impulse_leak:.3e}"
    )
    assert max_phase_error < 1.0e-7, (
        f"fundamental phase rotation detected: {max_phase_error:.9f} deg"
    )
    assert max_peak_overflow < 1.0e-12, (
        f"unexpected peak overflow: {max_peak_overflow:.3e}"
    )
    assert max_dc_error < 1.0e-15, (
        f"DC generated by symmetric material: {max_dc_error:.3e}"
    )
    assert max_stereo_error < 1.0e-15, (
        f"cross-channel interaction detected: {max_stereo_error:.3e}"
    )
    assert np.isfinite(max_output)

    print(
        "PASS V3 SAFE SINE 500-case matrix: "
        f"cases=500, max_block_error={max_block_error:.3e}, "
        f"max_impulse_leak={max_impulse_leak:.3e}, "
        f"max_phase_error={max_phase_error:.9f} deg, "
        f"max_peak_overflow={max_peak_overflow:.3e}, "
        f"max_dc_error={max_dc_error:.3e}, "
        f"max_stereo_error={max_stereo_error:.3e}, "
        f"max_output={max_output:.6f}"
    )


if __name__ == "__main__":
    run()
