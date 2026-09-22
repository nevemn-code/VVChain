import numpy as np
from scipy import signal

# 50-case isolation matrix for ANALOG interaction behaviour.
# It compares the complete reference-stage path WITH and WITHOUT ANALOG.
# The comparison is deliberately made after the same EQ/dynamic stages so
# any extra sample delay introduced by ANALOG is isolated.

VERSION = "v1"

def analog(x, amount, solid_state):
    if VERSION == "v1":
        drive = (1.10 if solid_state else 0.95) + 1.25 * amount
        u = np.clip(x, -1.20, 1.20)
        endpoint = max(1.0 - np.exp(-drive), 1.0e-6)
        shaped = np.sign(u) * (1.0 - np.exp(-drive * np.abs(u))) / endpoint
        return x + 0.55 * amount * (shaped - u)
    if VERSION == "v2":
        u = np.clip(x, -1.0, 1.0)
        shaped = np.sin(np.pi * 0.5 * u)
        return x + 0.50 * amount * (shaped - u)
    if VERSION == "v3":
        u = np.clip(x, -1.0, 1.0)
        u2 = u * u
        t3 = 4.0 * u * u2 - 3.0 * u
        t5 = 16.0 * u * u2 * u2 - 20.0 * u * u2 + 5.0 * u
        h3 = 0.020 if solid_state else 0.014
        h5 = 0.006 if solid_state else 0.004
        shaped = u + amount * (h3 * (t3 - u) + h5 * (t5 - u))
        return x + 0.90 * (shaped - u)
    raise RuntimeError(VERSION)

def reference_path(sr, freq, amp, color, solid_state, with_analog):
    n = 8192
    t = np.arange(n) / sr
    x = amp * np.sin(2.0 * np.pi * freq * t)

    # Representative stable EQ section.
    cutoff = min(0.45, 3000.0 / (sr / 2.0))
    sos = signal.butter(2, cutoff, output="sos")
    y = signal.sosfilt(sos, x)

    if with_analog:
        y = analog(y, color, solid_state)

    # Representative zero-lookahead dynamic stage. The exact same detector
    # state is used in both paths; only ANALOG differs.
    env = 0.0
    attack = np.exp(-1.0 / (0.001 * 1.0 * sr))
    release = np.exp(-1.0 / (0.001 * 50.0 * sr))
    z = np.empty_like(y)
    for i, sample in enumerate(y):
        coeff = attack if abs(sample) > env else release
        env = coeff * env + (1.0 - coeff) * abs(sample)
        gain = 1.0 if env < 0.2 else 0.8
        z[i] = sample * gain

    return x[2048:], z[2048:]

def phase_delta(a, b, sr, freq):
    t = np.arange(a.size) / sr
    ref = np.exp(-2j * np.pi * freq * t)
    pa = np.angle(np.sum(a * ref))
    pb = np.angle(np.sum(b * ref))
    return np.angle(np.exp(1j * (pb - pa)))

def run():
    sample_rates = [44100, 48000, 88200, 96000]
    freqs = [40, 100, 250, 800, 1500, 3000, 6000, 10000, 16000, 20000]
    colors = [0.0, 0.15, 0.35, 0.60, 1.0]
    modes = [False, True]
    amps = [0.03, 0.10, 0.30, 0.70, 0.95]

    cases = []
    for i in range(50):
        cases.append((
            sample_rates[i % len(sample_rates)],
            freqs[i % len(freqs)],
            colors[(i // 10) % len(colors)],
            modes[(i // 20) % len(modes)],
            amps[i % len(amps)],
        ))

    max_phase_deg = 0.0
    max_peak = 0.0

    for index, (sr, freq, color, solid_state, amp) in enumerate(cases, 1):
        baseline_in, baseline = reference_path(
            sr, freq, amp, color, solid_state, False)
        _, processed = reference_path(
            sr, freq, amp, color, solid_state, True)

        corr = signal.correlate(
            processed, baseline, mode="full", method="fft")
        lag = int(np.argmax(corr) - (baseline.size - 1))

        phase_deg = abs(np.degrees(
            phase_delta(baseline, processed, sr, freq)))
        peak = float(np.max(np.abs(processed)))

        # Static symmetry check: an odd ANALOG stage must not create DC bias.
        probe = np.linspace(-1.0, 1.0, 257)
        odd_error = float(np.max(
            np.abs(analog(probe, color, solid_state)
                   + analog(-probe, color, solid_state))))

        assert np.isfinite(processed).all(), f"NaN/Inf case {index}"
        assert lag == 0, f"ANALOG added sample delay in case {index}: {lag}"
        assert phase_deg < 0.25, (
            f"phase deviation too large in case {index}: {phase_deg:.4f} deg")
        assert odd_error < 1.0e-6, (
            f"odd-symmetry/DC error in case {index}: {odd_error}")
        assert peak < 1.35, (
            f"unexpected peak build-up in case {index}: {peak}")

        max_phase_deg = max(max_phase_deg, phase_deg)
        max_peak = max(max_peak, peak)

    print(
        f"✅ {VERSION.upper()} ANALOG 50-case full-chain isolation matrix passed; "
        f"max phase delta={max_phase_deg:.6f} deg, max peak={max_peak:.6f}, "
        f"sample lag=0 in all 50 cases"
    )

if __name__ == "__main__":
    run()
