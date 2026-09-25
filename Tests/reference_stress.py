#!/usr/bin/env python3
from __future__ import annotations

import argparse
import math
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]


def analog_reference(x, amount, solid_state, x2=1.0):
    x=np.asarray(x,dtype=np.float64);amount=float(np.clip(amount,0.0,0.60));x2=float(np.clip(x2,1.0,2.0))
    if amount<=0.0:return x.copy()
    alpha=amount*(1.80 if solid_state else 1.55);norm=(1+alpha)**.25;shaped=np.clip(x,-1.0,1.0)
    def f(v):return v/(1+alpha*v*v)**.25*norm
    def F(v):return (2/(3*alpha))*((1+alpha*v*v)**.75-1)*norm
    y=np.empty_like(shaped);prev=0.;has=False
    for i,sample in enumerate(shaped):
        if not has:sat=f(sample);has=True
        else:
            d=sample-prev;sat=f(.5*(sample+prev)) if abs(d)<1e-7 else (F(sample)-F(prev))/d
        y[i]=x[i]+(sat-sample)*x2;prev=sample
    return y

def tape_reference(x, degree):
    x = np.asarray(x, dtype=np.float64)
    depth = float(np.clip(degree, 0.0, 100.0)) / 100.0
    if depth <= 0.0:
        return x.copy()

    drive = max(1.0, 1.0 + 1.5 * depth)
    makeup = 1.0 / max(math.tanh(drive), 1.0e-6)
    driven = np.tanh(x * drive) * makeup
    return x + (driven - x) * depth


def q_from_wheel(q, delta, fine=False):
    q = max(0.1, float(q))
    wheel_units = max(-1.0, min(1.0, float(delta)))
    speed = 0.0075 if fine else 0.075
    return max(0.1, min(18.0, q * math.exp(wheel_units * speed)))


def time_coeff(sample_rate, ms):
    sample_rate = max(8000.0, float(sample_rate))
    ms = max(0.001, float(ms))
    return math.exp(-1.0 / (0.001 * ms * sample_rate))


def source_guards():
    cpp=(ROOT/"Source"/"DSP"/"ChainDSP.cpp").read_text(encoding="utf-8")
    editor=(ROOT/"Source"/"PluginEditor.cpp").read_text(encoding="utf-8")
    adaa=(ROOT/"Source"/"DSP"/"VVChain_AnalogADAA_v2.h").read_text(encoding="utf-8")
    for marker in ("VVChain_AnalogADAA_v2","analogADAA[band][ch].processSample","const double shapingInput = juce::jlimit(-1.0, 1.0, x)","const double delta = saturated - shapingInput","constexpr double kSmoothingMs = 0.25"):
        assert marker in cpp or marker in adaa,marker
    assert "std::sqrt(std::sqrt(1.0 + alpha))" in adaa
    assert "calcAntiderivative" in adaa
    assert "x2Multiplier[band] = p.eqColorX2[band] ? 2.0 : 1.0" in cpp
    assert "const float speed = fine ? 0.0075f : 0.075f;" in editor


def run_stress_test(iterations=5):
    source_guards()
    rng = np.random.default_rng(0x56564348)

    for iteration in range(max(1, iterations)):
        x = rng.uniform(-1.25, 1.25, size=32768)

        for solid_state in (False, True):
            for amount in (0.0, 0.05, 0.20, 0.40, 0.60):
                for x2 in (1.0, 2.0):
                    y = analog_reference(x, amount, solid_state, x2)
                    yn = analog_reference(-x, amount, solid_state, x2)
                    assert np.all(np.isfinite(y))
                    assert np.max(np.abs(y + yn)) < 1.0e-10
                    if amount == 0.0:
                        assert np.array_equal(y, x)
                    else:
                        probe=np.linspace(-1.0,1.0,4097)
                    alpha=amount*(1.80 if solid_state else 1.55)
                    static_y=probe/(1+alpha*probe*probe)**.25*(1+alpha)**.25
                    assert np.min(np.abs(static_y)-np.abs(probe))>=-1e-12

        for degree in (0.0, 6.0, 25.0, 50.0, 90.0):
            y = tape_reference(x, degree)
            assert np.all(np.isfinite(y))
            if degree == 0.0:
                assert np.array_equal(y, x)
            plus = tape_reference(np.array([1.0]), degree)[0]
            minus = tape_reference(np.array([-1.0]), degree)[0]
            assert abs(plus - 1.0) < 1.0e-12
            assert abs(minus + 1.0) < 1.0e-12

        deltas = np.linspace(-1.0, 1.0, 101)
        q_values = [q_from_wheel(1.0, d) for d in deltas]
        assert len({round(v, 12) for v in q_values}) == len(q_values)
        assert all(q_values[i] < q_values[i + 1] for i in range(len(q_values) - 1))

        for sr in (44100, 48000, 96000, 192000):
            for ms in (0.25, 5.0, 20.0, 120.0, 2000.0):
                coeff = time_coeff(sr, ms)
                assert math.isfinite(coeff)
                assert 0.0 < coeff < 1.0

    print(
        f"PASS DSP reference stress: {max(1, iterations)} iterations; "
        "Analog odd/no-shrink/dry, TAPE COLOR normalized unity, "
        "continuous Q and envelope coefficient checks"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--iterations", type=int, default=5)
    args = parser.parse_args()
    run_stress_test(max(1, args.iterations))
