#!/usr/bin/env python3
from __future__ import annotations

import math
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]

def log_cosh(z: float) -> float:
    a = abs(z)
    return a + math.log1p(math.exp(-2.0 * a)) - math.log(2.0)

def type_a_direct(x: np.ndarray, drive: float) -> np.ndarray:
    norm = 1.0 / max(math.tanh(drive), 1.0e-12)
    return np.tanh(x * drive) * norm

def type_a_adaa(x: np.ndarray, drive: float) -> np.ndarray:
    norm = 1.0 / max(math.tanh(drive), 1.0e-12)
    y = np.empty_like(x, dtype=np.float64)
    prev = 0.0
    has_prev = False

    def f(v: float) -> float:
        return math.tanh(drive * v) * norm

    def F(v: float) -> float:
        return log_cosh(drive * v) * norm / drive

    for i, sample in enumerate(np.asarray(x, dtype=np.float64)):
        if not has_prev:
            out = f(float(sample))
            has_prev = True
        else:
            delta = float(sample) - prev
            out = f(0.5 * (float(sample) + prev)) if abs(delta) < 1.0e-7 else (F(float(sample)) - F(prev)) / delta
        y[i] = out
        prev = float(sample)
    return y

def peak_db(spec: np.ndarray, freqs: np.ndarray, target: float) -> float:
    idx = int(np.argmin(np.abs(freqs - target)))
    lo, hi = max(0, idx - 2), min(len(spec), idx + 3)
    return 20.0 * math.log10(float(np.max(np.abs(spec[lo:hi]))) + 1.0e-30)

def main() -> None:
    cpp = (ROOT / "Source" / "DSP" / "ChainDSP.cpp").read_text(encoding="utf-8")
    header = (ROOT / "Source" / "DSP" / "ChainDSP.h").read_text(encoding="utf-8")
    web = (ROOT / "docs" / "vvchain-worklet.js").read_text(encoding="utf-8")

    assert "processTypeAAdAA" in cpp + header
    assert "logCosh" in cpp
    assert "typeAAdAA" in web
    assert "Math.log1p(Math.exp(-2*a))" in web

    for drive in (1.0, 1.15, 1.4, 1.75):
        norm = 1.0 / math.tanh(drive)
        assert abs(math.tanh(drive) * norm - 1.0) < 1.0e-12
        assert abs(math.tanh(-drive) * norm + 1.0) < 1.0e-12

    fs = 48000.0
    n = 32768
    t = np.arange(n, dtype=np.float64) / fs
    x = 0.8 * np.sin(2.0 * math.pi * 10000.0 * t)
    drive = 1.75

    direct = type_a_direct(x, drive)
    adaa = type_a_adaa(x, drive)
    assert np.all(np.isfinite(adaa))

    discard = 2048
    window = np.hanning(n - discard)
    direct_fft = np.fft.rfft(direct[discard:] * window)
    adaa_fft = np.fft.rfft(adaa[discard:] * window)
    freqs = np.fft.rfftfreq(len(window), 1.0 / fs)

    # 10 kHz odd harmonics fold to 18 kHz (3rd) and 2 kHz (5th) at 48 kHz.
    reduction_18k = peak_db(direct_fft, freqs, 18000.0) - peak_db(adaa_fft, freqs, 18000.0)
    reduction_2k = peak_db(direct_fft, freqs, 2000.0) - peak_db(adaa_fft, freqs, 2000.0)
    assert reduction_18k > 6.0, reduction_18k
    assert reduction_2k > 6.0, reduction_2k

    print(f"PASS Type-A ADAA anti-alias: 18k={reduction_18k:.2f} dB, 2k={reduction_2k:.2f} dB reduction")

if __name__ == "__main__":
    main()
