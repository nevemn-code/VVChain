#!/usr/bin/env python3
"""VVChain v1.0.9 deterministic TPT Bell EQ regression.

50 static cases verify the Cytomic/Simper TPT Bell transfer function against an
independent double-precision RBJ peaking reference, including magnitude,
phase, centre-frequency gain and exact 0 dB identity.

50 stress cases drive four serial bands with per-sample parameter modulation and
reject NaN/Inf or unstable state growth.
"""

from __future__ import annotations

import math
import numpy as np


SEED = 20260924
STATIC_CASES = 50
STRESS_CASES = 50


def rbj_coeff(fs: float, f0: float, q: float, gain_db: float) -> np.ndarray:
    A = 10.0 ** (gain_db / 40.0)
    w0 = 2.0 * math.pi * f0 / fs
    c = math.cos(w0)
    s = math.sin(w0)
    alpha = s / (2.0 * q)
    a0 = 1.0 + alpha / A
    return np.array([
        (1.0 + alpha * A) / a0,
        (-2.0 * c) / a0,
        (1.0 - alpha * A) / a0,
        (-2.0 * c) / a0,
        (1.0 - alpha / A) / a0,
    ], dtype=np.float64)


def rbj_response(fs: float, f0: float, q: float, gain_db: float,
                 frequencies: np.ndarray) -> np.ndarray:
    b0, b1, b2, a1, a2 = rbj_coeff(fs, f0, q, gain_db)
    z = np.exp(-2.0j * math.pi * frequencies / fs)
    return (b0 + b1 * z + b2 * z * z) / (1.0 + a1 * z + a2 * z * z)


def tpt_response(fs: float, f0: float, q: float, gain_db: float,
                 frequencies: np.ndarray) -> np.ndarray:
    A = 10.0 ** (gain_db / 40.0)
    g = math.tan(math.pi * f0 / fs)
    k = 1.0 / (q * A)
    a1 = 1.0 / (1.0 + g * (g + k))
    a2 = g * a1
    a3 = g * a2
    m1 = k * (A * A - 1.0)

    output: list[complex] = []
    for frequency in frequencies:
        z = np.exp(2.0j * math.pi * frequency / fs)
        matrix = np.array([
            [z + 1.0 - 2.0 * a1, 2.0 * a2],
            [-2.0 * a2, z - 1.0 + 2.0 * a3],
        ], dtype=np.complex128)
        rhs = np.array([2.0 * a2, 2.0 * a3], dtype=np.complex128)
        ic1, ic2 = np.linalg.solve(matrix, rhs)
        v1 = a1 * ic1 + a2 * (1.0 - ic2)
        output.append(1.0 + m1 * v1)

    return np.asarray(output, dtype=np.complex128)


class TPTBellState:
    def __init__(self) -> None:
        self.ic1 = 0.0
        self.ic2 = 0.0
        self.a1 = 1.0
        self.a2 = 0.0
        self.a3 = 0.0
        self.m1 = 0.0

    def set(self, fs: float, f0: float, q: float, gain_db: float) -> None:
        safe_f = float(np.clip(f0, 20.0, fs * 0.45))
        safe_q = float(np.clip(q, 0.1, 18.0))
        safe_gain = float(np.clip(gain_db, -18.0, 18.0))
        A = 10.0 ** (safe_gain / 40.0)
        g = math.tan(math.pi * safe_f / fs)
        k = 1.0 / (safe_q * A)
        self.a1 = 1.0 / (1.0 + g * (g + k))
        self.a2 = g * self.a1
        self.a3 = g * self.a2
        self.m1 = k * (A * A - 1.0)

    def process(self, x: float) -> float:
        v3 = x - self.ic2
        v1 = self.a1 * self.ic1 + self.a2 * v3
        v2 = self.ic2 + self.a2 * self.ic1 + self.a3 * v3
        self.ic1 = 2.0 * v1 - self.ic1
        self.ic2 = 2.0 * v2 - self.ic2
        return x + self.m1 * v1


def static_tests(rng: np.random.Generator) -> tuple[float, float, float, float]:
    max_mag_error = 0.0
    max_phase_error = 0.0
    max_centre_error = 0.0
    max_identity_error = 0.0

    for _ in range(STATIC_CASES):
        fs = float(rng.choice([44100, 48000, 88200, 96000]))
        os_fs = fs * 4.0
        f0 = float(10.0 ** rng.uniform(
            math.log10(20.0),
            math.log10(min(20000.0, os_fs * 0.44)),
        ))
        q = float(10.0 ** rng.uniform(math.log10(0.1), math.log10(18.0)))
        gain_db = float(rng.uniform(-18.0, 18.0))
        frequencies = np.geomspace(20.0, min(20000.0, os_fs * 0.49), 257)

        tpt = tpt_response(os_fs, f0, q, gain_db, frequencies)
        rbj = rbj_response(os_fs, f0, q, gain_db, frequencies)

        mag_error = np.max(
            np.abs(20.0 * np.log10(np.abs(tpt))
                   - 20.0 * np.log10(np.abs(rbj)))
        )
        phase_error = np.max(np.abs(
            np.angle(np.exp(1.0j * (
                np.unwrap(np.angle(tpt)) - np.unwrap(np.angle(rbj))
            )))
        ))

        centre = tpt_response(os_fs, f0, q, gain_db, np.array([f0]))[0]
        centre_error = abs(20.0 * math.log10(abs(centre)) - gain_db)

        filter_state = TPTBellState()
        filter_state.set(os_fs, f0, q, 0.0)
        signal = rng.standard_normal(256)
        output = np.asarray([filter_state.process(float(x)) for x in signal])
        identity_error = float(np.max(np.abs(output - signal)))

        max_mag_error = max(max_mag_error, float(mag_error))
        max_phase_error = max(max_phase_error, float(phase_error))
        max_centre_error = max(max_centre_error, float(centre_error))
        max_identity_error = max(max_identity_error, identity_error)

    return (
        max_mag_error,
        max_phase_error,
        max_centre_error,
        max_identity_error,
    )


def modulation_tests(rng: np.random.Generator) -> tuple[int, float, float]:
    failures = 0
    worst_output = 0.0
    worst_state = 0.0

    for _ in range(STRESS_CASES):
        fs = float(rng.choice([44100, 48000, 96000]))
        os_fs = fs * 4.0
        length = 4096
        time = np.arange(length) / os_fs

        f_a = 10.0 ** rng.uniform(math.log10(25.0), math.log10(12000.0))
        f_b = 10.0 ** rng.uniform(math.log10(30.0), math.log10(18000.0))
        signal = (
            0.12 * np.sin(2.0 * math.pi * f_a * time)
            + 0.08 * np.sin(2.0 * math.pi * f_b * time + 0.7)
            + 0.008 * rng.standard_normal(length)
        )

        filters = [TPTBellState() for _ in range(4)]
        base = [
            [
                10.0 ** rng.uniform(
                    math.log10(20.0),
                    math.log10(min(18000.0, os_fs * 0.44)),
                ),
                10.0 ** rng.uniform(math.log10(0.1), math.log10(12.0)),
                float(rng.uniform(-12.0, 12.0)),
            ]
            for _ in range(4)
        ]

        for sample_index in range(length):
            value = float(signal[sample_index])

            for band, filt in enumerate(filters):
                base_f, base_q, base_gain = base[band]
                frequency = float(np.clip(
                    base_f * (
                        1.0
                        + 0.12 * math.sin(
                            2.0 * math.pi * sample_index
                            / (250.0 + band * 83.0)
                        )
                    ),
                    20.0,
                    os_fs * 0.44,
                ))
                q = float(np.clip(
                    base_q * (
                        1.0
                        + 0.25 * math.sin(
                            2.0 * math.pi * sample_index
                            / (341.0 + band * 71.0)
                        )
                    ),
                    0.1,
                    12.0,
                ))
                gain = float(np.clip(
                    base_gain
                    + 1.0 * math.sin(
                        2.0 * math.pi * sample_index
                        / (431.0 + band * 97.0)
                    ),
                    -12.0,
                    12.0,
                ))
                filt.set(os_fs, frequency, q, gain)
                value = filt.process(value)

            if not math.isfinite(value):
                failures += 1
                break

            worst_output = max(worst_output, abs(value))
            state_values = [
                abs(filt.ic1) for filt in filters
            ] + [
                abs(filt.ic2) for filt in filters
            ]
            worst_state = max(worst_state, max(state_values))

    return failures, worst_output, worst_state


def main() -> None:
    rng = np.random.default_rng(SEED)
    mag_error, phase_error, centre_error, identity_error = static_tests(rng)
    failures, worst_output, worst_state = modulation_tests(rng)

    print(f"STATIC_CASES={STATIC_CASES}")
    print(f"MAX_MAG_ERROR_DB={mag_error:.12g}")
    print(f"MAX_PHASE_ERROR_RAD={phase_error:.12g}")
    print(f"MAX_CENTRE_GAIN_ERROR_DB={centre_error:.12g}")
    print(f"MAX_0DB_IDENTITY_ERROR={identity_error:.12g}")
    print(f"STRESS_CASES={STRESS_CASES}")
    print(f"STRESS_NONFINITE_CASES={failures}")
    print(f"STRESS_WORST_OUTPUT={worst_output:.12g}")
    print(f"STRESS_WORST_STATE={worst_state:.12g}")

    assert mag_error < 1.0e-5
    assert phase_error < 1.0e-5
    assert centre_error < 1.0e-6
    assert identity_error == 0.0
    assert failures == 0
    assert worst_output < 10.0
    assert worst_state < 2.0


if __name__ == "__main__":
    main()
