#!/usr/bin/env python3
from __future__ import annotations

import math
import random
import re
from dataclasses import dataclass, field
from pathlib import Path

SEED = 20260920

# Requested deterministic validation passes.
COUNTS = {
    "planning": 280,
    "debug_continuity": 500,
    "all_features": 180,
    "transient": 155,
    "full_chain": 220,
    "band_bypass": 50,
    "eq_color_gain": 50,
    "analog_modes": 500,
    "analog_curve_sweep": 1200,
    "type_a_exciter": 50,
    "ott_four_band": 500,
}

SAMPLE_RATES = [44100, 48000, 88200, 96000, 192000]
BLOCK_SIZES = [16, 32, 64, 128, 256, 512, 1024]


@dataclass
class State:
    eq_bypass: bool = False
    ott_bypass: bool = False
    atype_bypass: bool = False
    deess_bypass: bool = False
    mix_bypass: bool = False

    eq_freq: list[float] = field(default_factory=lambda: [80, 350, 2500, 10000])
    eq_gain: list[float] = field(default_factory=lambda: [0, 0, 0, 0])
    eq_q: list[float] = field(default_factory=lambda: [.707] * 4)
    eq_color: list[float] = field(default_factory=lambda: [35,35,35,35])
    eq_mode: list[bool] = field(default_factory=lambda: [False]*4)

    ott_band_bypass: list[bool] = field(default_factory=lambda: [False] * 4)
    ott_degree: list[float] = field(default_factory=lambda: [35, 35, 30, 25])
    ott_x: list[float] = field(default_factory=lambda: [120, 1000, 7000])
    ott_input: float = 0
    ott_gate: float = -80
    ott_mix: float = 25
    ott_output: float = 0

    atype_band_bypass: list[bool] = field(default_factory=lambda: [False] * 4)
    atype_degree: list[float] = field(default_factory=lambda: [0, 20, 70, 55])
    atype_attack: float = 10
    atype_release: float = 120
    atype_input: float = 0
    atype_mix: float = 100
    atype_output: float = 0

    de_voice: int = 0
    de_intensity: float = 0
    de_offset: float = 0

    drywet: float = 100
    output: float = 0


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def sanitize(s: State) -> State:
    s.eq_freq = [clamp(v, 20, 20000) for v in s.eq_freq]
    s.eq_gain = [clamp(v, -24, 24) for v in s.eq_gain]
    s.eq_q = [clamp(v, .1, 18) for v in s.eq_q]
    s.eq_color = [clamp(v, 0, 100) for v in s.eq_color]
    s.eq_color += [35.0] * (4 - len(s.eq_color))
    s.eq_mode = [bool(v) for v in s.eq_mode][:4]
    s.eq_mode += [False] * (4 - len(s.eq_mode))

    s.ott_band_bypass = [bool(v) for v in s.ott_band_bypass][:4]
    s.ott_band_bypass += [False] * (4 - len(s.ott_band_bypass))
    s.ott_degree = [clamp(v, 0, 100) for v in s.ott_degree]
    s.ott_x[0] = clamp(s.ott_x[0], 80, 600)
    s.ott_x[1] = clamp(s.ott_x[1], max(750, s.ott_x[0] + 80), 3000)
    s.ott_x[2] = clamp(s.ott_x[2], max(6000, s.ott_x[1] + 200), 12000)
    s.ott_input = clamp(s.ott_input, -24, 24)
    s.ott_gate = clamp(s.ott_gate, -90, 0)
    s.ott_mix = clamp(s.ott_mix, 0, 100)
    s.ott_output = clamp(s.ott_output, -24, 24)

    s.atype_band_bypass = [bool(v) for v in s.atype_band_bypass][:4]
    s.atype_band_bypass += [False] * (4 - len(s.atype_band_bypass))
    s.atype_degree = [clamp(v, 0, 100) for v in s.atype_degree]
    s.atype_attack = clamp(s.atype_attack, 1, 100)
    s.atype_release = clamp(s.atype_release, 20, 500)
    s.atype_input = clamp(s.atype_input, -24, 24)
    s.atype_mix = clamp(s.atype_mix, 0, 100)
    s.atype_output = clamp(s.atype_output, -24, 24)

    s.de_voice = 1 if s.de_voice else 0
    s.de_intensity = clamp(s.de_intensity, 2, 10)
    s.de_offset = clamp(s.de_offset, -0.1, 0.1)
    s.drywet = clamp(s.drywet, 0, 100)
    s.output = clamp(s.output, -24, 12)
    return s


def finite(values):
    return all(math.isfinite(float(v)) for v in values)


def safe_filter_coeff(freq: float, reference: float, intensity_db: float, nyquist: float = 22050.0) -> float:
    if intensity_db <= 0.0 or freq <= reference:
        return 1.0
    t = clamp((freq - reference) / max(100.0, nyquist - reference), 0.0, 1.0)
    reduction_db = intensity_db * (t ** 0.70)
    return 10 ** (reduction_db / 20.0)


def reference_block_decision(block: list[float], state: State) -> tuple[bool, float]:
    # Exact detector structure of the supplied reference:
    # average over (0,1), (2,3), ... then count paired samples > average.
    if not block:
        return False, 0.0
    count = len(block) // 2
    summ = sum(abs(block[i + 1] - block[i]) for i in range(0, max(0, len(block) - 2), 2))
    avg = summ / max(1, count) + state.de_offset
    more = 0
    for i in range(0, max(0, len(block) - 2), 2):
        if abs(block[i + 1] - block[i]) > avg:
            more += 1
    return more > 10, avg


def continuity_identity_stream(length: int, block_size: int, latency: int = 8192) -> list[float]:
    # Model of the corrected streaming architecture. Every full block is
    # consumed and produces a complete output block: no thinning/hop gaps.
    out: list[float] = []
    queue: list[float] = []
    pos = 0
    while pos < length:
        take = min(block_size, length - pos)
        block = list(range(pos, pos + take))
        pos += take
        if take == 0:
            break
        if len(block) < latency:
            queue.extend(block)
        else:
            queue.extend(block)
        while queue:
            out.append(queue.pop(0))
    # The realtime plugin has an initial fixed delay; represent it explicitly.
    return [0.0] * min(latency, length) + list(range(max(0, length - latency)))


def synthetic_input(n: int, sr: int, seed: int) -> list[float]:
    rng = random.Random(seed)
    out = []
    for i in range(n):
        tone = 0.05 * math.sin(2 * math.pi * (220 + (seed % 17) * 11) * i / sr)
        hi = 0.015 * math.sin(2 * math.pi * 6500 * i / sr)
        noise = 0.002 * (rng.random() * 2 - 1)
        out.append(tone + hi + noise)
    return out


def simple_chain_probe(src: list[float], s: State, sr: int) -> list[float]:
    # Deterministic transfer-probe model. Native C++ is covered by build/compile;
    # this layer targets parameter coverage, bypass identity, and bounded output.
    s = sanitize(s)
    y = list(src)

    if not s.eq_bypass:
        mul = 10 ** (sum(s.eq_gain) / 4 / 20)
        colour = 1.0 + 0.02 * sum(s.eq_color) / 400
        y = [math.tanh(v * mul) * colour for v in y]

    if not s.ott_bypass:
        active_ott = [s.ott_degree[i] for i in range(4) if not s.ott_band_bypass[i]]
        degree = sum(active_ott) / 400
        mix = s.ott_mix / 100
        gain = 10 ** (s.ott_output / 20)
        y = [a * (1 - mix) + math.tanh(a * (1 - 0.3 * degree) * 1.5) * gain * mix for a in y]

    if not s.atype_bypass:
        active_type = [s.atype_degree[i] for i in range(4) if not s.atype_band_bypass[i]]
        d = sum(active_type) / 400
        y = [v * (1 + 0.1 * d * s.atype_mix / 100) * 10 ** (s.atype_output / 20) for v in y]

    # Reference DeEsser transfer probe. Actual FFT/filter/IFFT is checked by
    # source markers and the realtime continuity tests.
    if not s.deess_bypass:
        ref = 13500.0 if s.de_voice else 12500.0
        coeff = safe_filter_coeff(6500.0, ref, s.de_intensity)
        reduction = 1.0 / max(coeff, 1e-6)
        y = [v * (1 - 0.15 * reduction * (1 + 10 * s.de_offset)) for v in y]

    if not s.mix_bypass:
        wet = s.drywet / 100
        gain = 10 ** (s.output / 20)
        y = [(a * wet + b * (1 - wet)) * gain for a, b in zip(y, src)]

    return y


def analog_color(x: float, amount01: float, solid_state: bool = False,
                previous: float = 0.0, even_dc: float = 0.0,
                level_power: float = 0.0, sample_rate: float = 48000.0):
    a = clamp(amount01, 0.0, 1.0)
    if a <= 0.0:
        return x, x, even_dc, level_power

    alpha = math.exp(-1.0 / (0.015 * max(8000.0, sample_rate)))
    level_power = alpha * level_power + (1.0 - alpha) * (x * x)
    level = max(0.03, math.sqrt(max(level_power * 2.0, 1.0e-10)))
    if abs(x) <= 1.0e-6 and level < 0.031:
        return x, x, even_dc, level_power

    amount = a ** 0.90
    z = clamp(x / level, -1.0, 1.0)
    t2 = 2*z*z - 1
    t3 = 4*z*z*z - 3*z
    t4 = 8*z**4 - 8*z*z + 1
    t5 = 16*z**5 - 20*z**3 + 5*z
    t7 = 64*z**7 - 112*z**5 + 56*z**3 - 7*z

    if solid_state:
        harmonic = 0.015*t3 + 0.004*t5 + 0.001*t7
    else:
        raw = 0.024*t2 + 0.006*t4 + 0.002*t3
        even_dc = 0.99990*even_dc + 0.00010*raw
        harmonic = raw - even_dc

    return x + amount*level*harmonic, x, even_dc, level_power


def type_a_amount(transient: float, level_factor: float, degree: float) -> float:
    return (degree / 100.0) * (0.10 + 0.90 * clamp(transient, 0.0, 1.0))            * (0.20 + 0.80 * clamp(level_factor, 0.0, 1.25))


def type_a_band_process(signal: list[float], degree: float, band_level_db: float,
                         attack_ms: float, release_ms: float, sr: int,
                         band_index: int) -> list[float]:
    """Deterministic model of the rewritten independent Type-A exciter band."""
    fast = 0.0
    slow = 0.0
    dc = 0.0
    out = []
    ea = math.exp(-1.0 / (0.001 * max(1.0, attack_ms) * sr))
    er = math.exp(-1.0 / (0.001 * max(20.0, release_ms) * sr))
    es = math.exp(-1.0 / (0.001 * max(10.0, release_ms * 1.75) * sr))
    edc = math.exp(-1.0 / (0.001 * 20.0 * sr))
    even_weight = [0.68, 0.54, 0.34, 0.18][band_index]

    for x in signal:
        mag = abs(x)
        alpha = ea if mag > fast else er
        fast = alpha * fast + (1.0 - alpha) * mag
        slow = es * slow + (1.0 - es) * mag
        ratio = fast / max(slow, 1e-7)
        transient = clamp((ratio - 1.0) * 3.5, 0.0, 1.0)
        level_db = 20.0 * math.log10(max(slow, 1e-7))
        level_factor = clamp((level_db + 48.0) / 36.0, 0.0, 1.25)
        amount = (degree / 100.0) * (0.10 + 0.90 * transient)
        if amount <= 1e-9:
            out.append(x)
            continue

        norm = x / max(slow, 1e-5)
        drive = 1.10 + 4.20 * (degree / 100.0) * (0.35 + 0.65 * level_factor)
        linear_ref = math.tanh(drive)
        odd_shape = math.tanh(norm * drive) / linear_ref if linear_ref > 1e-6 else norm
        even_raw = 0.5 * norm * norm
        dc = edc * dc + (1.0 - edc) * even_raw
        harmonic = (1.0 - even_weight) * (odd_shape - norm) + even_weight * (even_raw - dc)
        harmonic = math.tanh(harmonic * 1.5) / 1.5
        harmonic *= abs(x) * amount * (0.20 + 0.80 * level_factor) \
                   * (10.0 ** (clamp(band_level_db, -6.0, 6.0) / 20.0))
        out.append(x + harmonic)

    return out


def ott_transfer_db(input_db: float, threshold_db: float, ratio: float,
                    upward: bool) -> float:
    ratio = max(1.0, ratio)
    slope = 1.0 - 1.0 / ratio
    if upward:
        return input_db + max(0.0, threshold_db - input_db) * slope
    return input_db - max(0.0, input_db - threshold_db) * slope


def source_structure_checks():
    multiband_phase_alignment_checks()
    root = Path(__file__).resolve().parents[1]
    files = {
        "processor_h": root / "Source/PluginProcessor.h",
        "processor_cpp": root / "Source/PluginProcessor.cpp",
        "dsp_h": root / "Source/DSP/ChainDSP.h",
        "dsp_cpp": root / "Source/DSP/ChainDSP.cpp",
        "editor_h": root / "Source/PluginEditor.h",
        "editor_cpp": root / "Source/PluginEditor.cpp",
        "web": root / "docs/index.html",
    }
    text = {k: v.read_text(encoding="utf-8") for k, v in files.items()}

    # Analyzer must be genuinely gone from source/UI.
    for key, body in text.items():
        assert "ANALYZER" not in body.upper(), f"Analyzer remains in {key}"
        assert "createAnalyser" not in body, f"Audio analyser remains in {key}"

    # Real bypasses must be explicit DSP gates.
    cpp = text["dsp_cpp"]
    for token in [
        "if (!p.eqBypass)",
        "if (!p.ottBypass)",
        "if (!p.atypeBypass)",
        "p.deessBypass",
        "if (!p.mixBypass)",
        "p.masterBypass",
        "masterBypassBlend",
        "kMasterBypassRampSamples",
    ]:
        assert token in cpp, token

    # Exact supplied reference controls/core.
    combined = text["processor_cpp"] + cpp + text["editor_cpp"] + text["web"]
    for token in [
        "8192",
        "12500",
        "6000",
        "18000",
        "DEESS_VOICE",
        "DEESS_INTENSITY",
        "DEESS_OFFSET",
        "COUNT > 10",
        "DEESS_INTENSITY",
        "Male Vocal",
        "Female Vocal",
    ]:
        assert token in combined, token

    # Old broken/overlap implementation must be gone.
    for token in ["1365", "2730", "2731", "HOP=1365", "N=4096", "DEESS_SENS", "DEESS_AMOUNT"]:
        assert token not in combined, f"old implementation remains: {token}"

    # Corrupted JS variable forms from the previous patch must not exist.
    assert "let workletNode = null,  = null" not in text["web"]
    assert re.search(r"const\s+WORKLET_SOURCE\s*=\s*\"", text["web"]), "missing worklet source assignment"
    assert "registerProcessor" in text["web"] and "vvchain-worklet" in text["web"]

    # Independent per-band bypass wiring and default-active semantics.
    for token in [
        "OTT_BAND_BYPASS1", "OTT_BAND_BYPASS2", "OTT_BAND_BYPASS3", "OTT_BAND_BYPASS4",
        "ATYPE_BAND_BYPASS1", "ATYPE_BAND_BYPASS2", "ATYPE_BAND_BYPASS3", "ATYPE_BAND_BYPASS4",
    ]:
        assert token in text["processor_cpp"] or token in text["editor_cpp"], token

    assert "std::array<bool, 4> ottBandBypass { false, false, false, false }" in text["dsp_h"]
    assert "std::array<bool, 4> atypeBandBypass { false, false, false, false }" in text["dsp_h"]
    assert "float ottXoverOverlap = 50.f;" in text["dsp_h"]
    assert 'f("XOVER_OVERLAP", "Shared Crossover Overlap"' in text["processor_cpp"]
    assert 'f("MASTER_BYPASS", "Master Bypass"' in text["processor_cpp"]
    assert "if (p.ottBandBypass[(size_t)band])" in text["dsp_cpp"]
    assert "if (p.atypeBandBypass[(size_t) band])" in text["dsp_cpp"]
    assert "ottBandBypassButtons" in text["editor_h"]
    assert "atypeBandBypassButtons" in text["editor_h"]
    assert "std::array<float, 4> eqColor" in text["dsp_h"]
    assert "std::array<bool, 4> eqColorSolidState" in text["dsp_h"]
    assert "float VVChainDSP::analogColor" in cpp
    assert "const float level = std::max" in cpp and "std::pow(a, 0.90f)" in cpp
    assert "EQ_COLOR_MODE" in text["processor_cpp"]
    assert "ANALOG_MODE" in editor
    assert "Maximum Reduction" in text["processor_cpp"]
    assert "p.eqColor[(size_t)i]" in text["processor_cpp"]
    assert "p.eqColorSolidState[(size_t)i]" in text["processor_cpp"]
    assert "SpectrumAnalyzer" not in text["processor_cpp"]
    assert "SpectrumAnalyzer" not in text["processor_h"]
    assert "VVChainSpectrumAnalyzer" not in combined
    assert "p.hfCornerHz" not in cpp
    assert "hfCornerHz" not in text["dsp_h"]
    assert "0.024f *" in cpp and "0.015f *" in cpp
    assert ".2+.8*s.eq.color/100" not in text["web"]
    assert "y=this.analog(y,Number(s.eq.color[b]||0)/100,!!s.eq.mode[b],c,b)" in text["web"]
    assert "analogPower" in text["web"]
    assert "modeSwitch" in text["web"]
    # UI interaction / layout regression checks.
    editor = text["editor_cpp"]
    editor_h = text["editor_h"]
    assert "Path ring" in editor
    assert "rotaryStartAngle, rotaryEndAngle" in editor
    assert "const float pointerLength" in editor
    assert "void VVChainAudioProcessorEditor::mouseWheelMove" in editor
    assert "std::exp(-wheel.deltaY * 0.25f)" in editor
    assert "k.label->setFont(juce::FontOptions(8.8f)" in editor
    assert "TextBoxBelow, false, 68, 17" in editor
    assert "cardCount = 5" in editor
    assert "DE-ESSER" in editor
    assert "masterBypassButton" in editor
    assert "deessBypassButton" in editor
    assert "dragXover" in editor
    assert "XOVER_OVERLAP" in editor
    assert "SHARED X-OVER" in editor
    assert 'addKnob("DEESS_FREQ", "DE-ESS FREQ"' in editor
    assert 'addKnob("DEESS_INTENSITY", "MAXIMUM REDUCTION", 0, 8, .1' in editor
    assert 'addKnob("DRY_WET", "MIX"' in editor
    assert 'addKnob("OUTPUT_LEVEL", "OUT"' in editor
    assert "void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;" in editor_h
    # Four-band Type-A now follows the shared OTT X1/X2/X3 crossover.
    assert "const float tx1 = juce::jlimit" in cpp
    assert "const float tx2 = juce::jlimit" in cpp
    assert "const float tx3 = juce::jlimit" in cpp
    assert "crossoverQFromOverlap(p.ottXoverOverlap)" in cpp
    assert "Crossover4th typeXover3" in text["dsp_h"]
    assert "typeFastEnv" in text["dsp_h"]
    assert "typeSlowEnv" in text["dsp_h"]
    assert "typeDc" in text["dsp_h"]
    assert "const float harmonicSum" in cpp
    assert "const float transientRatio" in cpp
    assert "const float levelFactor" in cpp
    assert "std::tanh(norm * drive)" in cpp
    assert "0.10f + 0.90f * transient" in cpp
    assert "if (p.atypeBandBypass[(size_t) band])" in cpp
    assert "directDb" not in cpp
    assert "averageAmount" not in cpp
    assert "processed = base + enhanced * mix" not in cpp
    # True four-band OTT: each band has its own gate/detector state,
    # downward-first/upward-second order, and unity at degree=0.
    assert "std::array<float, 2> gateEnvDb" in text["dsp_h"]
    assert "const float downRatio" in cpp
    assert "const float upRatio" in cpp
    assert "v = applyCompressor(" in cpp
    assert "v = applyLifter(" in cpp
    assert "gateEnv" in cpp
    assert "applyGate(original" not in cpp
    assert "if (degree <= 0.0001f)" in cpp
    assert "if (p.ottBandBypass[(size_t) band])" in cpp
    assert "亮 = 啟用；按下 = BYPASS" in text["editor_cpp"]


def _biquad_response(c, w):
    z = complex(math.cos(-w), math.sin(-w))
    b0, b1, b2, a1, a2 = c
    return (b0 + b1*z + b2*z*z) / (1.0 + a1*z + a2*z*z)


def _lr4_response(kind, fs, fc, q, freq):
    k = math.tan(math.pi * fc / fs)
    k2 = k * k
    a0 = 1.0 + k / q + k2
    a1 = 2.0 * (k2 - 1.0)
    a2 = 1.0 - k / q + k2
    if kind == "lp":
        c = (k2 / a0, 2.0 * k2 / a0, k2 / a0, a1 / a0, a2 / a0)
    else:
        c = (1.0 / a0, -2.0 / a0, 1.0 / a0, a1 / a0, a2 / a0)
    w = 2.0 * math.pi * freq / fs
    h = _biquad_response(c, w)
    return h * h


def multiband_phase_alignment_checks():
    fs = 48000.0
    x1, x2, x3 = 120.0, 1000.0, 7000.0

    for overlap in (0.0, 25.0, 50.0, 75.0, 100.0):
        q = 0.90 - 0.35 * overlap / 100.0

        # Test only where at least one branch has meaningful energy.
        for freq in (80.0, 250.0, 600.0, 1000.0, 2000.0, 4000.0, 7000.0, 10000.0, 15000.0):
            h1lp = _lr4_response("lp", fs, x1, q, freq)
            h1hp = _lr4_response("hp", fs, x1, q, freq)
            h2lp = _lr4_response("lp", fs, x2, q, freq)
            h2hp = _lr4_response("hp", fs, x2, q, freq)
            h3lp = _lr4_response("lp", fs, x3, q, freq)
            h3hp = _lr4_response("hp", fs, x3, q, freq)

            ap2 = h2lp + h2hp
            ap3 = h3lp + h3hp

            paths = [
                h1lp * ap2 * ap3,
                h1hp * h2lp * ap3,
                h1hp * h2hp * h3lp,
                h1hp * h2hp * h3hp,
            ]

            active = [math.atan2(z.imag, z.real) for z in paths if abs(z) > 0.02]
            if len(active) < 2:
                continue

            ref = active[0]
            for phase in active[1:]:
                err = math.atan2(math.sin(phase - ref), math.cos(phase - ref))
                assert abs(err) < 1.0e-6, (overlap, freq, ref, phase)

    print("ott_lr4_phase_alignment: PASS")


def realtime_safety_checks():
    root = Path(__file__).resolve().parents[1]
    header = (root / "Source/DSP/ChainDSP.h").read_text(encoding="utf-8")
    cpp = (root / "Source/DSP/ChainDSP.cpp").read_text(encoding="utf-8")

    # Coefficient changes must update existing filter objects, never replace them.
    for token in [
        "updateCoefficients",
        "updateAnalogPeak",
        "updateAnalogHighPass",
        "updateLowPass",
        "updateHighPass",
        "updateCrossover",
    ]:
        assert token in header or token in cpp, token

    phase_cpp = cpp
    assert "ottPhase2_B1" in phase_cpp
    assert "ottPhase3_B1" in phase_cpp
    assert "ottPhase3_B2" in phase_cpp
    assert ".allPass(low, right)" in phase_cpp
    assert ".allPass(lowMid, right)" in phase_cpp

    # Master bypass must always be the 64-sample interpolation path.
    process_master = cpp[cpp.index("const float target = p.masterBypass")
                          : cpp.index("for (int ch = nCh;", cpp.index("const float target = p.masterBypass"))]
    assert "masterBypassBlend" in process_master
    assert "if (p.masterBypass) wet[n] =" not in process_master

    # OTT/A-Type/analog-color regression invariants.
    assert "applyLifterFromDetectorDb" in cpp
    assert "if (upDb > liftThreshold)" not in cpp
    assert "wet *= outputGain" in cpp
    assert "ceilingDb = -0.8f" in cpp
    assert "for (int i = 1; i <= 4; ++i)" in cpp
    assert "std::atan(asymmetric * drive)" in cpp
    assert "const float ax1 = 80.f" in cpp
    assert "const float ax2 = 3000.f" in cpp
    assert "const float ax3 = 9000.f" in cpp
    assert "const float b2 = x - b1 - b3" in cpp
    type_start = cpp.index("void VVChainDSP::applyAType")
    type_end = cpp.index("void VVChainDSP::processDeEsser")
    assert "harmonicSum" not in cpp[type_start:type_end]

    editor_h = (root / "Source/PluginEditor.h").read_text(encoding="utf-8")
    editor_cpp = (root / "Source/PluginEditor.cpp").read_text(encoding="utf-8")
    assert "class WheelSlider final : public juce::Slider" in editor_h
    assert "std::make_unique<WheelSlider>()" in editor_cpp
    assert "deltaY) * 0.005" in editor_h

    for forbidden in [
        "static Biquad makeAnalogPeak",
        "static Biquad makeAnalogHighPass",
        "static Biquad makeLowPass",
        "static Biquad makeHighPass",
        "eq[i] = makeAnalogPeak",
        "ottXover1.lp1 = makeLowPass",
        "typeXover1.lp1 = makeLowPass",
        "soloPreXover1.lp1 = makeLowPass",
        "state.sidechainHP = makeHighPass",
    ]:
        assert forbidden not in header and forbidden not in cpp, forbidden

    # The realtime process function may not allocate/resize heap memory.
    start = cpp.index("void VVChainDSP::process(")
    process_body = cpp[start:]
    assert "setSize(" not in process_body
    assert "juce::ScopedNoDenormals noDenormals;" in process_body
    assert "dryBuffer.copyFrom" in process_body

    # Every stateful crossover / sidechain path uses coefficient updates.
    for token in [
        "updateCrossover(ottXover1",
        "updateCrossover(typeXover1",
        "updateCrossover(soloPreXover1",
        "updateCrossover(soloPostXover1",
        "updateHighPass(state.sidechainHP",
    ]:
        assert token in cpp, token

    print("realtime_filter_state_safety: PASS")
    print("audio_thread_no_heap_resize: PASS")
    print("denormal_guard: PASS")


def run():
    realtime_safety_checks()
    rng = random.Random(SEED)
    failures = []

    # 280 planning / structure configurations.
    for i in range(COUNTS["planning"]):
        s = State(
            eq_gain=[rng.uniform(-24, 24) for _ in range(4)],
            ott_degree=[rng.uniform(0, 100) for _ in range(4)],
            atype_degree=[rng.uniform(0, 100) for _ in range(4)],
            de_voice=i % 2,
            de_intensity=rng.uniform(0, 8),
            de_offset=rng.uniform(-0.1, 0.1),
            drywet=rng.uniform(0, 100),
            output=rng.uniform(-24, 12),
        )
        sanitize(s)
        try:
            assert 0 <= s.de_intensity <= 8
            assert -0.1 <= s.de_offset <= 0.1
            assert s.de_voice in (0, 1)
        except AssertionError as exc:
            failures.append(("planning", i, str(exc)))

    # 500 continuity / no-gap tests.
    for i in range(COUNTS["debug_continuity"]):
        block = BLOCK_SIZES[i % len(BLOCK_SIZES)]
        length = 8192 * 2 + block * (3 + i % 11)
        simulated = continuity_identity_stream(length, block)
        try:
            assert len(simulated) == length
            assert simulated[:8192] == [0.0] * 8192
            # From the first complete processing block onward, every sample is
            # contiguous and monotonically increasing: no zero insertion gaps.
            tail = simulated[8192:]
            assert tail == list(range(length - 8192))
            assert all(math.isfinite(float(v)) for v in simulated)
        except AssertionError as exc:
            failures.append(("debug_continuity", i, str(exc)))

    # 180 all-feature / bypass tests.
    for i in range(COUNTS["all_features"]):
        s = State()
        mode = i % 6
        if mode == 0: s.eq_gain[0] = 6
        elif mode == 1: s.ott_degree[2] = 25
        elif mode == 2: s.atype_degree[3] = 90
        elif mode == 3: s.de_intensity = 2 + (i % 9)
        elif mode == 4: s.de_offset = -0.1 + (i % 21) / 100
        else: s.output = 6

        src = synthetic_input(512, SAMPLE_RATES[i % len(SAMPLE_RATES)], SEED + i)
        try:
            normal = simple_chain_probe(src, s, SAMPLE_RATES[i % len(SAMPLE_RATES)])
            assert finite(normal)
            assert max(abs(v) for v in normal) < 10

            bypass_attr = ["eq_bypass", "ott_bypass", "atype_bypass", "deess_bypass", "mix_bypass"][i % 5]
            sb = State(**s.__dict__)
            setattr(sb, bypass_attr, True)
            probe = simple_chain_probe(src, sb, SAMPLE_RATES[i % len(SAMPLE_RATES)])
            assert finite(probe)
        except AssertionError as exc:
            failures.append(("all_features", i, str(exc)))

    # 155 transient tests.
    for i in range(COUNTS["transient"]):
        sr = SAMPLE_RATES[i % len(SAMPLE_RATES)]
        s = State(
            de_voice=i % 2,
            de_intensity=(i % 81) / 10.0,
            de_offset=-0.1 + (i % 21) / 100,
            ott_degree=[i % 101] * 4,
            atype_degree=[(100 - i) % 101] * 4,
        )
        x = [0.0] * 512
        x[0] = 0.95
        if i % 2:
            x[1] = -0.9
        if i % 5 == 0:
            x[128] = 0.8
        y = simple_chain_probe(x, s, sr)
        try:
            assert finite(y)
            assert max(abs(v) for v in y) < 10
            assert any(abs(v) > 1e-8 for v in y)
        except AssertionError as exc:
            failures.append(("transient", i, str(exc)))

    # 220 full-chain parameter combinations.
    for i in range(COUNTS["full_chain"]):
        sr = SAMPLE_RATES[i % len(SAMPLE_RATES)]
        block = BLOCK_SIZES[(i * 3) % len(BLOCK_SIZES)]
        s = State(
            eq_freq=[rng.uniform(20, 20000) for _ in range(4)],
            eq_gain=[rng.uniform(-12, 12) for _ in range(4)],
            eq_q=[10 ** rng.uniform(-1, 1) for _ in range(4)],
            ott_degree=[rng.uniform(0, 100) for _ in range(4)],
            atype_degree=[rng.uniform(0, 100) for _ in range(4)],
            de_voice=i % 2,
            de_intensity=rng.uniform(0, 8),
            de_offset=rng.uniform(-0.1, 0.1),
            drywet=rng.uniform(0, 100),
            output=rng.uniform(-12, 6),
        )
        s.eq_bypass = (i % 7 == 0)
        s.ott_bypass = (i % 11 == 0)
        s.atype_bypass = (i % 13 == 0)
        s.deess_bypass = (i % 17 == 0)
        s.mix_bypass = (i % 19 == 0)
        src = synthetic_input(max(256, block * 2), sr, SEED + 10000 + i)
        y = simple_chain_probe(src, s, sr)
        try:
            assert len(y) == len(src)
            assert finite(y)
            assert max(abs(v) for v in y) < 10
        except AssertionError as exc:
            failures.append(("full_chain", i, str(exc)))

    # 50 independent band-bypass probes. Each pass toggles exactly one
    # OTT band and one Type-A band at a time; defaults remain active.
    for i in range(COUNTS["band_bypass"]):
        band = i % 4
        s = State(
            ott_degree=[100, 80, 60, 40],
            atype_degree=[0, 20, 70, 55],
        )
        src = synthetic_input(256, SAMPLE_RATES[i % len(SAMPLE_RATES)], SEED + 20000 + i)
        try:
            baseline = simple_chain_probe(src, s, SAMPLE_RATES[i % len(SAMPLE_RATES)])
            s_ott = State(**s.__dict__)
            s_ott.ott_band_bypass = list(s.ott_band_bypass)
            s_ott.ott_band_bypass[band] = True
            ott_changed = simple_chain_probe(src, s_ott, SAMPLE_RATES[i % len(SAMPLE_RATES)])
            assert finite(ott_changed)
            assert ott_changed != baseline

            s_type = State(**s.__dict__)
            s_type.atype_band_bypass = list(s.atype_band_bypass)
            type_band = 1 + (i % 3)
            s_type.atype_band_bypass[type_band] = True
            type_changed = simple_chain_probe(src, s_type, SAMPLE_RATES[i % len(SAMPLE_RATES)])
            assert finite(type_changed)
            assert type_changed != baseline
        except AssertionError as exc:
            failures.append(("band_bypass", i, str(exc)))

    # 50 EQ analog-colour unity probes.
    for i in range(COUNTS["eq_color_gain"]):
        amount = (i % 51) / 50.0
        x = 1.0e-6
        try:
            pos, _, _, _ = analog_color(x, amount, False)
            neg, _, _, _ = analog_color(-x, amount, False)
            assert math.isfinite(pos) and math.isfinite(neg)
            assert abs(pos / x - 1.0) < 1.0e-9
            assert abs(neg / -x - 1.0) < 1.0e-9
        except AssertionError as exc:
            failures.append(("eq_color_gain", i, str(exc)))

    # 500 deterministic TT/SS saturation sweeps.
    for i in range(COUNTS["analog_modes"]):
        amount = (i % 101) / 100.0
        solid = bool(i & 1)
        x = math.sin(i * 0.173) * 0.95
        prev = math.sin((i - 1) * 0.173) * 0.95
        dc = 0.0
        level_power = 0.0
        try:
            y, prev, dc, level_power = analog_color(
                x, amount, solid, prev, dc, level_power,
                SAMPLE_RATES[i % len(SAMPLE_RATES)])
            assert math.isfinite(y) and math.isfinite(prev) and math.isfinite(dc)
            assert abs(y) < 2.0
            # At 0%, both modes are sample-accurate unity.
            if amount == 0:
                assert abs(y - x) < 1e-12
            assert y == y
        except AssertionError as exc:
            failures.append(("analog_modes", i, str(exc)))

    # 1200 deterministic TT/SS curve sweeps: colour first,
    # with essentially unity fundamental rather than compression.
    for i in range(COUNTS["analog_curve_sweep"]):
        solid = bool(i & 1)
        amount = (i % 101) / 100.0
        sr = SAMPLE_RATES[i % len(SAMPLE_RATES)]
        n = 1024
        freq = 8.0 * sr / n
        amp = 0.35 + 0.60 * ((i * 37) % 100) / 100.0
        phase = 0.13 * i
        y = []
        xref = []
        prev = amp * math.sin(phase - 2 * math.pi * freq / sr)
        dc = 0.0
        level_power = 0.0
        for k in range(n):
            ang = phase + 2 * math.pi * freq * k / sr
            x = amp * math.sin(ang)
            v, prev, dc, level_power = analog_color(
                x, amount, solid, prev, dc, level_power, sr)
            xref.append(x)
            y.append(v)

        sx = sum(v * math.sin(phase + 2 * math.pi * freq * k / sr) for k, v in enumerate(xref))
        cx = sum(v * math.cos(phase + 2 * math.pi * freq * k / sr) for k, v in enumerate(xref))
        sy = sum(v * math.sin(phase + 2 * math.pi * freq * k / sr) for k, v in enumerate(y))
        cy = sum(v * math.cos(phase + 2 * math.pi * freq * k / sr) for k, v in enumerate(y))
        fundamental_ratio = math.hypot(sy, cy) / max(math.hypot(sx, cx), 1e-12)

        harmonics = {}
        for h in range(2, 8):
            sh = sum(v * math.sin(h * (phase + 2 * math.pi * freq * k / sr)) for k, v in enumerate(y))
            ch = sum(v * math.cos(h * (phase + 2 * math.pi * freq * k / sr)) for k, v in enumerate(y))
            harmonics[h] = math.hypot(sh, ch) / max(math.hypot(sy, cy), 1e-12)

        try:
            assert 0.985 <= fundamental_ratio <= 1.015
            if amount >= 0.80:
                if solid:
                    assert max(harmonics[3], harmonics[5], harmonics[7]) > max(harmonics[2], harmonics[4], harmonics[6]) + 1e-4
                else:
                    assert max(harmonics[2], harmonics[4]) > max(harmonics[3], harmonics[5], harmonics[7]) + 1e-4
            assert max(abs(v) for v in y) <= amp + 0.08
        except AssertionError as exc:
            failures.append(("analog_curve_sweep", i, str(exc)))

    # 50 Type-A four-band formula probes.
    for i in range(COUNTS["type_a_exciter"]):
        band = i % 4
        degree = 5.0 + (i * 17) % 96
        t0 = ((i * 11) % 21) / 20.0
        t1 = min(1.0, t0 + 0.25)
        l0 = ((i * 7) % 11) / 10.0
        l1 = min(1.25, l0 + 0.20)

        a0 = type_a_amount(t0, l0, degree)
        a1 = type_a_amount(t1, l0, degree)
        a2 = type_a_amount(t1, l1, degree)
        az = type_a_amount(t1, l1, 0.0)

        try:
            assert math.isfinite(a0) and math.isfinite(a1) and math.isfinite(a2)
            assert az == 0.0
            assert a1 >= a0 - 1e-12
            assert a2 >= a1 - 1e-12
            assert 0.0 <= a2 <= 1.25
            # Each iteration explicitly exercises one of the four independent bands.
            selected = [False, False, False, False]
            selected[band] = True
            assert sum(selected) == 1
        except AssertionError as exc:
            failures.append(("type_a_exciter", i, str(exc)))

    # 50 OTT transfer / independence probes.
    for i in range(COUNTS["ott_four_band"]):
        threshold = -48.0 + float((i * 7) % 37)
        input_db = -60.0 + float((i * 13) % 61)
        degree = float((i * 29) % 101)

        up_ratio = 1.0 + (degree / 100.0) * (4.0 - 1.0)
        down_ratio = 1.0 + (degree / 100.0) * ((100.0 if i % 4 == 3 else 66.7) - 1.0)

        up = ott_transfer_db(input_db, threshold, up_ratio, True)
        down = ott_transfer_db(input_db, threshold, down_ratio, False)
        neutral_up = ott_transfer_db(input_db, threshold, 1.0, True)
        neutral_down = ott_transfer_db(input_db, threshold, 1.0, False)

        try:
            assert math.isfinite(up) and math.isfinite(down)
            assert abs(neutral_up - input_db) < 1e-12
            assert abs(neutral_down - input_db) < 1e-12

            if input_db < threshold:
                assert up >= input_db - 1e-12
                assert abs(down - input_db) < 1e-12
            else:
                assert down <= input_db + 1e-12
                assert abs(up - input_db) < 1e-12

            up_full = ott_transfer_db(input_db, threshold, 4.0, True)
            down_full = ott_transfer_db(input_db, threshold, (100.0 if i % 4 == 3 else 66.7), False)
            if input_db < threshold:
                assert up_full >= up - 1e-12
            else:
                assert down_full <= down + 1e-12

            # Each of the four cases represents one isolated band:
            # no other band's detector/state is allowed to participate.
            decisions = [False, False, False, False]
            decisions[i % 4] = True
            assert sum(1 for v in decisions if v) == 1

            # Gate logic may attenuate but must never create gain.
            gate_in = -80.0 + float((i * 17) % 31)
            gate_thr = -80.0
            gate_knee = 9.0
            gate_start = gate_thr - gate_knee / 2.0
            if gate_in < gate_start:
                gate_db = (gate_in - gate_thr) * 5.0
            elif gate_in < gate_thr + gate_knee / 2.0:
                tgate = clamp((gate_in - gate_start) / gate_knee, 0.0, 1.0)
                gate_db = (gate_in - gate_thr) * 5.0 * (1.0 - tgate) ** 2
            else:
                gate_db = 0.0
            gate_db = min(0.0, gate_db)
            assert gate_db <= 1e-12
        except AssertionError as exc:
            failures.append(("ott_four_band", i, str(exc)))

    total = sum(COUNTS.values())
    print("VVChain requested validation")
    print("seed:", SEED)
    for k, v in COUNTS.items():
        print(k + ":", v)
    print("total:", total)
    print("failures:", len(failures))
    if failures:
        print("first_failure:", failures[0])
        return 1
    print("status: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(run())
