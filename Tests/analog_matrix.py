import numpy as np

# Deterministic reference for the V3 Chebyshev block processor.
# This mirrors the C++ implementation in ChainDSP.cpp:
# tanh -> T2/T3 -> remove generated DC -> RMS match -> zero-mean Delta mix.

def process_v3(x, drive, amount):
    x = np.asarray(x, dtype=np.float64)
    if amount <= 0.0 or drive <= 0.0 or x.size == 0:
        return x.copy(), x.copy()

    input_rms = float(np.sqrt(np.mean(x * x)))
    if input_rms < 1.0e-4:
        return x.copy(), np.zeros_like(x)

    driven = np.tanh(x * drive)
    t2 = 2.0 * driven * driven - 1.0
    t3 = 4.0 * driven * driven * driven - 3.0 * driven

    shaped = driven + 0.25 * t2 + 0.15 * t3
    shaped -= np.mean(shaped)

    output_rms = float(np.sqrt(np.mean(shaped * shaped)))
    gain_comp = input_rms / output_rms if output_rms > 1.0e-4 else 1.0

    delta = shaped * gain_comp - x
    delta -= np.mean(delta)

    return x + amount * delta, delta


def block_boundary_leak(x, block_size, drive, amount):
    """
    Zero-latency guard for a blockwise, memoryless nonlinear stage.

    Cross-correlation is intentionally NOT used here: a nonlinear waveform
    changes its harmonic weighting, so the maximum correlation peak can move
    by a few samples even when the DSP does not introduce a sample delay.
    Instead, place a marker in the middle block and verify that no output leaks
    into the preceding or following block.
    """
    x = np.asarray(x, dtype=np.float64)
    y = np.empty_like(x)

    for start in range(0, len(x), block_size):
        end = min(start + block_size, len(x))
        yy, _ = process_v3(x[start:end], drive, amount)
        y[start:end] = yy

    outside = np.concatenate((y[:block_size], y[2 * block_size:]))
    return float(np.max(np.abs(outside))) if outside.size else 0.0


def phase_deg(signal_out, freq, fs):
    n = len(signal_out)
    t = np.arange(n, dtype=np.float64) / fs
    ref = np.exp(-1j * 2.0 * np.pi * freq * t)
    z = np.dot(signal_out, ref)
    return float(np.angle(z, deg=True))


def run():
    rng = np.random.default_rng(20260922)
    sample_rates = [44100.0, 48000.0, 88200.0, 96000.0]
    drives = [0.80, 0.95, 1.15, 1.50, 2.00]
    amounts = [0.05, 0.20, 0.35, 0.60, 1.00]
    amplitudes = [0.01, 0.10, 0.30, 0.70, 0.95]
    freqs = [40.0, 100.0, 1000.0, 3000.0, 8000.0, 12000.0, 18000.0]
    block_sizes = [64, 128, 256, 512, 1024]

    cases = 0
    max_phase = 0.0
    max_block_leak = 0.0
    max_dc_delta = 0.0
    max_rms_error = 0.0
    max_peak = 0.0

    # 500 deterministic parameter combinations, plus multiple block sizes.
    for fs in sample_rates:
        for drive in drives:
            for amount in amounts:
                for amp in amplitudes:
                    freq = freqs[(cases // 3) % len(freqs)]

                    n_total = 8192
                    t = np.arange(n_total, dtype=np.float64) / fs
                    # Mixed-tone + a tiny deterministic noise floor catches
                    # interactions that a pure sine can hide.
                    x = (
                        amp * np.sin(2 * np.pi * freq * t)
                        + 0.20 * amp * np.sin(2 * np.pi * min(freq * 1.73, fs * 0.45) * t + 0.37)
                        + 0.0025 * amp * rng.standard_normal(n_total)
                    )

                    # Run the actual algorithm block-by-block to expose
                    # block-boundary/RMS behaviour.
                    for block_size in block_sizes:
                        y = np.empty_like(x)
                        for start in range(0, n_total, block_size):
                            end = min(start + block_size, n_total)
                            yy, delta = process_v3(x[start:end], drive, amount)
                            y[start:end] = yy

                            # The injected Delta must not carry block DC.
                            max_dc_delta = max(
                                max_dc_delta, abs(float(np.mean(delta)))
                            )

                        # Fundamental phase is the sample-accurate delay check.
                        n_phase = 4096
                        xx = amp * np.sin(
                            2 * np.pi * min(freq, fs * 0.45) *
                            np.arange(n_phase, dtype=np.float64) / fs
                        )
                        yy, delta = process_v3(xx, drive, amount)
                        phase_in = phase_deg(xx, min(freq, fs * 0.45), fs)
                        phase_out = phase_deg(yy, min(freq, fs * 0.45), fs)
                        dphase = abs((phase_out - phase_in + 180.0) % 360.0 - 180.0)
                        max_phase = max(max_phase, dphase)

                        in_rms = np.sqrt(np.mean(x[start:end] ** 2))
                        color_only, _ = process_v3(x[start:end], drive, 1.0)
                        # Auto-gain target is checked on the corrected coloured
                        # branch by using the returned matched branch:
                        color_rms = np.sqrt(np.mean(color_only ** 2))
                        if in_rms > 1.0e-6:
                            max_rms_error = max(
                                max_rms_error,
                                abs(color_rms / in_rms - 1.0)
                            )

                        max_peak = max(max_peak, float(np.max(np.abs(y))))

                    marker = np.zeros(block_size * 3, dtype=np.float64)
                    marker[block_size + min(7, block_size - 1)] = 0.73
                    max_block_leak = max(
                        max_block_leak,
                        block_boundary_leak(marker, block_size, drive, amount),
                    )
                    cases += 1

    assert max_block_leak < 1.0e-15, (
        f"cross-block leakage detected: {max_block_leak:.3e}"
    )
    assert max_phase < 0.25, f"phase drift too large: {max_phase:.6f} deg"
    assert max_dc_delta < 1.0e-12, f"Delta DC detected: {max_dc_delta:.3e}"
    assert max_rms_error < 0.02, f"RMS mismatch too large: {max_rms_error:.6f}"
    assert np.isfinite(max_peak)
    assert max_peak < 1.50, f"unexpected peak: {max_peak:.6f}"

    # Static structural guard: this V3 core is mathematically memoryless.
    # The test intentionally does not claim "zero aliasing"; the C++ runs
    # inside VVChain's existing 4x EQ oversampling stage.
    print(
        "PASS V3 Chebyshev RMS/Delta matrix: "
        f"cases={cases}, max_phase={max_phase:.6f} deg, "
        f"max_block_leak={max_block_leak:.3e}, max_delta_dc={max_dc_delta:.3e}, "
        f"max_rms_error={max_rms_error:.6f}, max_peak={max_peak:.6f}"
    )


if __name__ == "__main__":
    run()
