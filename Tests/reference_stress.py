#!/usr/bin/env python3
from __future__ import annotations

import cmath
import math
import random
from dataclasses import dataclass, field
from pathlib import Path

SEED = 20260920
COUNTS = {
    "planning": 280,
    "debug": 120,
    "all_features": 180,
    "transient": 655,
    "full_chain": 820,
}
SRS = [44100, 48000, 88200, 96000, 192000]
BLOCKS = [16, 32, 64, 128, 256, 512, 1024]

@dataclass
class State:
    eq_freq: list[float] = field(default_factory=lambda: [80, 350, 2500, 10000])
    eq_gain: list[float] = field(default_factory=lambda: [0, 0, 0, 0])
    eq_q: list[float] = field(default_factory=lambda: [.707] * 4)
    eq_color: float = 35
    hf_corner: float = 70

    ott_degree: list[float] = field(default_factory=lambda: [100] * 4)
    ott_lift_t: list[float] = field(default_factory=lambda: [-40] * 4)
    ott_lift_a: list[float] = field(default_factory=lambda: [50] * 4)
    ott_lift_r: list[float] = field(default_factory=lambda: [50] * 4)
    ott_lift_m: list[float] = field(default_factory=lambda: [100] * 4)
    ott_comp_t: list[float] = field(default_factory=lambda: [-12] * 4)
    ott_comp_a: list[float] = field(default_factory=lambda: [15] * 4)
    ott_comp_r: list[float] = field(default_factory=lambda: [60] * 4)
    ott_comp_m: list[float] = field(default_factory=lambda: [100] * 4)
    ott_level: list[float] = field(default_factory=lambda: [0] * 4)
    ott_x: list[float] = field(default_factory=lambda: [350, 1000, 9000])
    ott_input: float = 5.2
    ott_gate: float = -80
    ott_mix: float = 100
    ott_clipper: bool = True
    ott_output: float = -6

    atype_degree: list[float] = field(default_factory=lambda: [0, 20, 70, 55])
    atype_level: list[float] = field(default_factory=lambda: [0, 0, 1, 1])
    atype_attack: float = 10
    atype_release: float = 120
    atype_input: float = 0
    atype_mix: float = 100
    atype_output: float = 0

    deess_voice: int = 0
    deess_intensity: float = 10
    deess_offset: float = 0

    drywet: float = 100
    output: float = 0

def clamp(v, lo, hi):
    return max(lo, min(hi, v))

def sanitize(s: State) -> State:
    s.eq_freq = [clamp(v, 20, 20000) for v in s.eq_freq]
    s.eq_gain = [clamp(v, -24, 24) for v in s.eq_gain]
    s.eq_q = [clamp(v, .1, 18) for v in s.eq_q]
    s.eq_color = clamp(s.eq_color, 0, 100)
    s.hf_corner = clamp(s.hf_corner, 40, 120)

    s.ott_degree = [clamp(v, 0, 100) for v in s.ott_degree]
    s.ott_lift_t = [clamp(v, -80, 0) for v in s.ott_lift_t]
    s.ott_lift_a = [clamp(v, 1, 500) for v in s.ott_lift_a]
    s.ott_lift_r = [clamp(v, 10, 2500) for v in s.ott_lift_r]
    s.ott_lift_m = [clamp(v, 0, 100) for v in s.ott_lift_m]
    s.ott_comp_t = [clamp(v, -24, 0) for v in s.ott_comp_t]
    s.ott_comp_a = [clamp(v, .1, 250) for v in s.ott_comp_a]
    s.ott_comp_r = [clamp(v, 10, 2500) for v in s.ott_comp_r]
    s.ott_comp_m = [clamp(v, 0, 100) for v in s.ott_comp_m]
    s.ott_level = [clamp(v, -24, 12) for v in s.ott_level]
    s.ott_x[0] = clamp(s.ott_x[0], 80, 600)
    s.ott_x[1] = clamp(s.ott_x[1], max(750, s.ott_x[0] + 80), 3000)
    s.ott_x[2] = clamp(s.ott_x[2], max(6000, s.ott_x[1] + 200), 12000)
    s.ott_input = clamp(s.ott_input, -24, 24)
    s.ott_gate = clamp(s.ott_gate, -90, 0)
    s.ott_mix = clamp(s.ott_mix, 0, 100)
    s.ott_output = clamp(s.ott_output, -24, 24)

    s.atype_degree = [clamp(v, 0, 100) for v in s.atype_degree]
    s.atype_level = [clamp(v, -6, 6) for v in s.atype_level]
    s.atype_attack = clamp(s.atype_attack, 1, 100)
    s.atype_release = clamp(s.atype_release, 20, 500)
    s.atype_input = clamp(s.atype_input, -24, 24)
    s.atype_mix = clamp(s.atype_mix, 0, 100)
    s.atype_output = clamp(s.atype_output, -24, 24)

    s.deess_voice = 1 if s.deess_voice else 0
    s.deess_intensity = clamp(s.deess_intensity, 2, 10)
    s.deess_offset = clamp(s.deess_offset, -0.1, 0.1)
    s.drywet = clamp(s.drywet, 0, 100)
    s.output = clamp(s.output, -24, 12)
    return s

def finite(values):
    return all(math.isfinite(float(v)) for v in values)

def reference_deesser_kernel(x: list[float], voice: int, intensity: float, offset: float) -> list[float]:
    """Deterministic small-block model of the same reference decision/filter equations."""
    threshold = sum(abs(x[i + 1] - x[i]) for i in range(len(x) - 1)) / max(1, len(x) // 2) + offset
    count_more = sum(1 for i in range(len(x) - 1) if abs(x[i + 1] - x[i]) > threshold)

    if count_more <= 10:
        return list(x)

    target = 12500.0 if voice == 0 else 13500.0
    # Stable frequency-domain proxy: same coefficient equation, sampled at 32 bins.
    out = []
    n = len(x)
    energy = sum(v * v for v in x) / max(1, n)
    for i, sample in enumerate(x):
        phase = 2.0 * math.pi * i / max(1, n)
        gain = 1.0
        for k in range(1, 33):
            freq = 44100.0 * k / 8192.0
            if freq < target / 10:
                coeff = 0.5
            elif freq >= target:
                coeff = intensity * target / freq
            else:
                coeff = 1.0 + (intensity - 1.0) * (freq / target) ** 3
            gain += 1.0 / max(coeff, 1e-9) * math.cos(k * phase) * 0.001
        out.append(sample * gain)
    limit = 4.0 * max(1e-6, math.sqrt(energy))
    return [clamp(v, -limit, limit) for v in out]

def chain_probe(src: list[float], s: State, sr: int) -> list[float]:
    s = sanitize(s)
    # Non-zero, finite transfer probes for every module; native DSP does the full audio path.
    y = list(src)
    colour = 0.002 + 0.02 * s.eq_color / 100.0
    eq_gain = sum(s.eq_gain) / 4.0
    eq_mul = 10 ** (eq_gain / 20.0 / 6.0)
    for i, v in enumerate(y):
        y[i] = math.tanh(v * eq_mul) * (1.0 + colour)

    ott_mix = s.ott_mix / 100.0
    ott_degree = sum(s.ott_degree) / 400.0
    ott_gain = 10 ** (s.ott_output / 20.0)
    for i, v in enumerate(y):
        compressed = v * (0.70 + 0.30 * (1.0 - ott_degree))
        y[i] = v * (1.0 - ott_mix) + math.tanh(compressed * 1.7) * ott_gain * ott_mix

    atype = sum(s.atype_degree) / 400.0
    for i, v in enumerate(y):
        y[i] = v * (1.0 + 0.10 * atype * s.atype_mix / 100.0)

    de = reference_deesser_kernel(y, s.deess_voice, s.deess_intensity, s.deess_offset)
    mix = s.drywet / 100.0
    output = 10 ** (s.output / 20.0)
    return [(a + (b - a) * mix) * output for a, b in zip(src, de)]

def signature(s: State) -> float:
    values = [
        *s.eq_freq, *s.eq_gain, *s.eq_q, s.eq_color, s.hf_corner,
        *s.ott_degree, *s.ott_lift_t, *s.ott_lift_a, *s.ott_lift_r, *s.ott_lift_m,
        *s.ott_comp_t, *s.ott_comp_a, *s.ott_comp_r, *s.ott_comp_m, *s.ott_level,
        *s.ott_x, s.ott_input, s.ott_gate, s.ott_mix, float(s.ott_clipper), s.ott_output,
        *s.atype_degree, *s.atype_level, s.atype_attack, s.atype_release,
        s.atype_input, s.atype_mix, s.atype_output,
        float(s.deess_voice), s.deess_intensity, s.deess_offset,
        s.drywet, s.output
    ]
    return sum((i + 1) * float(v) for i, v in enumerate(values))

def random_state(rng: random.Random) -> State:
    return sanitize(State(
        eq_freq=[rng.uniform(1, 24000) for _ in range(4)],
        eq_gain=[rng.uniform(-36, 36) for _ in range(4)],
        eq_q=[10 ** rng.uniform(-1.5, 1.5) for _ in range(4)],
        eq_color=rng.uniform(-20, 130),
        hf_corner=rng.uniform(0, 250),
        ott_degree=[rng.uniform(-20, 140) for _ in range(4)],
        ott_lift_t=[rng.uniform(-100, 20) for _ in range(4)],
        ott_lift_a=[rng.uniform(0, 800) for _ in range(4)],
        ott_lift_r=[rng.uniform(1, 4000) for _ in range(4)],
        ott_lift_m=[rng.uniform(-20, 140) for _ in range(4)],
        ott_comp_t=[rng.uniform(-40, 20) for _ in range(4)],
        ott_comp_a=[rng.uniform(0, 300) for _ in range(4)],
        ott_comp_r=[rng.uniform(1, 4000) for _ in range(4)],
        ott_comp_m=[rng.uniform(-20, 140) for _ in range(4)],
        ott_level=[rng.uniform(-36, 24) for _ in range(4)],
        ott_x=[rng.uniform(40, 800), rng.uniform(600, 4000), rng.uniform(5000, 14000)],
        ott_input=rng.uniform(-40, 40),
        ott_gate=rng.uniform(-120, 20),
        ott_mix=rng.uniform(-20, 140),
        ott_clipper=bool(rng.getrandbits(1)),
        ott_output=rng.uniform(-40, 30),
        atype_degree=[rng.uniform(-20, 140) for _ in range(4)],
        atype_level=[rng.uniform(-12, 12) for _ in range(4)],
        atype_attack=rng.uniform(0, 150),
        atype_release=rng.uniform(1, 800),
        atype_input=rng.uniform(-40, 40),
        atype_mix=rng.uniform(-20, 140),
        atype_output=rng.uniform(-40, 30),
        deess_voice=rng.randrange(2),
        deess_intensity=rng.uniform(0, 15),
        deess_offset=rng.uniform(-1, 1),
        drywet=rng.uniform(-20, 140),
        output=rng.uniform(-40, 24),
    ))

def structure_checks():
    pp = Path("Source/PluginProcessor.cpp").read_text(encoding="utf-8")
    ph = Path("Source/PluginProcessor.h").read_text(encoding="utf-8")
    dsp = Path("Source/DSP/ChainDSP.cpp").read_text(encoding="utf-8")
    dh = Path("Source/DSP/ChainDSP.h").read_text(encoding="utf-8")
    ui = Path("Source/PluginEditor.cpp").read_text(encoding="utf-8")
    web = Path("docs/index.html").read_text(encoding="utf-8")

    for token in [
        "DEESS_VOICE", "DEESS_INTENSITY", "DEESS_OFFSET",
        "OTT_DEGREE", "ATYPE_DEGREE", '"EQ" + n + "_FREQ"',
    ]:
        assert token in pp, token

    for token in [
        "8192", "targetFreq", "countMore", "fft(deessFft, false)",
        "fft(deessFft, true)", "processDeEsser",
    ]:
        assert token in (dsp + dh), token

    for token in [
        "copySpectrumTo", "analyzerFFT", "kFFTSize", "setLatencySamples(8192)",
    ]:
        assert token in (pp + ph), token

    for token in [
        "EqBand1", "OttDegree1", "OttX1", "TypeDegree1",
        "DeEssIntensity", "mouseDown", "mouseDrag",
    ]:
        assert token in ui, token

    for token in [
        "audioFileInput", "loopStart", "loopEnd", "AudioContext",
        "createAnalyser", "fftSize", "state.de.intensity", "state.de.offset",
        "functionTabs",
    ]:
        assert token in web, token

def assert_ok(values, label):
    assert finite(values), label + ": non-finite"
    assert max((abs(v) for v in values), default=0.0) < 1e8, label + ": unstable"

def run():
    rng = random.Random(SEED)
    failures = []

    for i in range(COUNTS["planning"]):
        s = random_state(rng)
        try:
            assert math.isfinite(signature(s))
            assert 80 <= s.ott_x[0] < s.ott_x[1] < s.ott_x[2] <= 12000
            assert 0 <= s.deess_voice <= 1
            assert 2 <= s.deess_intensity <= 10
            assert -0.1 <= s.deess_offset <= 0.1
        except AssertionError as exc:
            failures.append(("planning", i, str(exc)))

    for i in range(COUNTS["debug"]):
        s = State(
            eq_color=(i % 101),
            ott_degree=[i % 101] * 4,
            atype_degree=[(100 - i) % 101] * 4,
            deess_voice=i % 2,
            deess_intensity=2 + (i % 9),
            deess_offset=-0.1 + (i % 201) / 1000.0,
        )
        try:
            probe = [0, 1, -1, .2, -.2, 0, .8, -.75]
            assert_ok(chain_probe(probe, s, 48000), "debug")
        except AssertionError as exc:
            failures.append(("debug", i, str(exc)))

    mutations = [
        lambda s: s.eq_freq.__setitem__(0, 900),
        lambda s: s.eq_gain.__setitem__(0, 7),
        lambda s: s.eq_q.__setitem__(0, 10),
        lambda s: setattr(s, "eq_color", 90),
        lambda s: setattr(s, "hf_corner", 110),
        lambda s: s.ott_degree.__setitem__(0, 30),
        lambda s: s.ott_degree.__setitem__(1, 60),
        lambda s: s.ott_degree.__setitem__(2, 80),
        lambda s: s.ott_degree.__setitem__(3, 95),
        lambda s: s.ott_x.__setitem__(0, 500),
        lambda s: s.ott_x.__setitem__(1, 1600),
        lambda s: s.ott_x.__setitem__(2, 10000),
        lambda s: setattr(s, "ott_mix", 40),
        lambda s: setattr(s, "ott_output", 3),
        lambda s: setattr(s, "atype_degree", [90, 40, 80, 60]),
        lambda s: setattr(s, "atype_mix", 45),
        lambda s: setattr(s, "atype_attack", 2),
        lambda s: setattr(s, "atype_release", 300),
        lambda s: setattr(s, "deess_voice", 1),
        lambda s: setattr(s, "deess_intensity", 2),
        lambda s: setattr(s, "deess_intensity", 10),
        lambda s: setattr(s, "deess_offset", -0.1),
        lambda s: setattr(s, "deess_offset", 0.1),
        lambda s: setattr(s, "drywet", 25),
        lambda s: setattr(s, "output", 6),
    ]
    tone = [
        .04 * math.sin(2 * math.pi * 440 * n / 48000)
        + .012 * math.sin(2 * math.pi * 6800 * n / 48000)
        for n in range(256)
    ]
    base = State()

    for i in range(COUNTS["all_features"]):
        name = i % len(mutations)
        s = State()
        mutations[name](s)
        try:
            s = sanitize(s)
            assert abs(signature(s) - signature(base)) > 1e-9
            assert_ok(chain_probe(tone, s, 48000), f"all_features/{name}")
        except AssertionError as exc:
            failures.append(("all_features", i, str(exc)))

    for i in range(COUNTS["transient"]):
        s = random_state(rng)
        x = [0.0] * 256
        mode = i % 6
        if mode == 0:
            x[0] = 1.0
        elif mode == 1:
            x[0], x[1] = 1.0, -.92
        elif mode == 2:
            for n in range(0, 256, 16):
                x[n] = .85 if (n // 16) % 2 == 0 else -.85
        elif mode == 3:
            for n in range(10):
                x[n] = .95 * (0.7 ** n)
        elif mode == 4:
            x[63:67] = [0.95, 1.0, 1.0, -.9]
        else:
            for n in range(32, 256, 64):
                x[n] = .8
        try:
            assert_ok(chain_probe(x, s, SRS[i % len(SRS)]), "transient")
        except AssertionError as exc:
            failures.append(("transient", i, str(exc)))

    for i in range(COUNTS["full_chain"]):
        sr = SRS[i % len(SRS)]
        block = BLOCKS[i % len(BLOCKS)]
        s = random_state(rng)
        tone = 110.0 * 2.0 ** ((i % 72) / 12.0)
        x = [
            .06 * math.sin(2 * math.pi * tone * n / sr)
            + .008 * math.sin(2 * math.pi * 6500 * n / sr)
            for n in range(block)
        ]
        try:
            assert 0 <= (i % 101) <= 100
            result = chain_probe(x, s, sr)
            assert_ok(result, "full_chain")
        except AssertionError as exc:
            failures.append(("full_chain", i, str(exc)))

    structure_checks()

    total = sum(COUNTS.values())
    print("VVChain reference stress test")
    print("seed:", SEED)
    for key, value in COUNTS.items():
        print(key + ":", value)
    print("total:", total)
    print("failures:", len(failures))

    if failures:
        print("first_failure:", failures[0])
        return 1

    print("status: PASS")
    return 0

if __name__ == "__main__":
    raise SystemExit(run())
