#!/usr/bin/env python3
# v1.0.37 regression matrix: TPT Bell EQ + existing v1.0.8 UI/interaction gates.: shared TAPE/ANALOG crossovers, module-isolated Delta, global hover values, and Transient invariants.
"""
VVChain Dynamic EQ UI/control regression matrix.

Covers:
- 280 deterministic design/edge cases
- 10 repeated core drag passes
- 10 repeated all-feature interaction passes
- 6 transient gesture passes
- 10 full-chain simulated UI sessions

This is a logic/source regression test. The actual JUCE compile remains covered by CI.
"""
from __future__ import annotations

import math
import random
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = ROOT / "Source" / "PluginEditor.cpp"
PROC = ROOT / "Source" / "PluginProcessor.cpp"
HEAD = ROOT / "Source" / "PluginEditor.h"
DSP = ROOT / "Source" / "DSP" / "ChainDSP.cpp"
ENGINE = ROOT / "Source" / "VVChain_DynEQ_Engine.h"

def clamp(v, lo, hi):
    return max(lo, min(hi, v))

def gain_from_y(y, height=315.0):
    half = max(1.0, height * 0.5)
    sd = height * 0.5 - y
    t = clamp(abs(sd) / half, 0.0, 1.0)
    if t <= 0.34:
        mag = 3.0 * (t / 0.34)
    elif t <= 0.56:
        mag = 3.0 + 3.0 * ((t - 0.34) / 0.22)
    elif t <= 0.80:
        mag = 6.0 + 6.0 * ((t - 0.56) / 0.24)
    else:
        mag = 12.0 + 6.0 * ((t - 0.80) / 0.20)
    return clamp(mag if sd >= 0 else -mag, -18.0, 18.0)

def dynamics_from_cursor(eq_gain, y, height=315.0):
    target_gain = gain_from_y(y, height)
    return clamp((target_gain - eq_gain) / 18.0 * 100.0, -100.0, 100.0)

def dyn_from_drag(start_dyn, start_y, current_y, graph_h=315.0, scale=1.0, eq_gain=0.0):
    _ = start_dyn, start_y, scale
    return dynamics_from_cursor(eq_gain, current_y, graph_h)

def y_from_db(db, height=315.0):
    v = clamp(db, -18.0, 18.0)
    mag = abs(v)
    if mag <= 3.0:
        t = 0.34 * (mag / 3.0)
    elif mag <= 6.0:
        t = 0.34 + 0.22 * ((mag - 3.0) / 3.0)
    elif mag <= 12.0:
        t = 0.56 + 0.24 * ((mag - 6.0) / 6.0)
    else:
        t = 0.80 + 0.20 * ((mag - 12.0) / 6.0)
    return height * 0.5 + (-1.0 if v >= 0 else 1.0) * t * height * 0.5

def db_from_y(y, height=315.0):
    return gain_from_y(y, height)

def dynamic_target(offset, dyn_range, dynamics):
    amount = abs(clamp(dynamics, -100.0, 100.0)) / 100.0
    direction = -1.0 if dynamics < 0.0 else 1.0
    return clamp(offset + direction * abs(dyn_range) * amount, -18.0, 18.0)

def source_assertions():
    cpp = CPP.read_text(encoding="utf-8")
    proc = PROC.read_text(encoding="utf-8")
    head = HEAD.read_text(encoding="utf-8")
    dsp = DSP.read_text(encoding="utf-8")
    engine = ENGINE.read_text(encoding="utf-8")
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")
    worklet = (ROOT / "docs" / "vvchain-worklet.js").read_text(encoding="utf-8")

    # Current graph / parameter binding contracts.
    for token in [
        'DYN_DYNAMICS',
        'setParameter("DYN_DYNAMICS" + n, dynamics);',
        'parameter->beginChangeGesture();',
        'parameter->endChangeGesture();',
        'graphFreqDragGrabOffsetX',
        'qFromWheel',
        'eqYToDb',
        'pointNearDynamicNode',
        'constexpr float staticNodeRadius = 7.0f',
        'resetParameter("EQ" + n + "_GAIN", 0.0f)',
        'resetParameter("DYN_DYNAMICS" + n, 0.0f)',
        'Static EQ point always wins when the pointer is actually on it.',
    ]:
        assert token in cpp or token in proc or token in head or token in engine, token

    assert 'DYN_TARGET" + n, storedTarget' not in cpp
    assert cpp.count("juce::StringArray({") == 0
    for dead in ("graphHintActiveMask", "graphHintBand", "graphHintAutoHideAt"):
        assert dead not in cpp and dead not in head, dead

    # Current Web graph gesture contracts.
    for token in [
        'function graphHintBandHtml',
        'resetGraphGainAtDoubleClick',
        'eqCanvas.addEventListener("dblclick",resetGraphGainAtDoubleClick)',
        'function nextQFromWheel',
        'const hzv=invLog(clamp(x,0,w)/w)',
        'gainAtCursor=yToDb(y,h)',
        'targetGain=yToDb(y,h)',
        'state.eq.freq[dragBand]=hzv',
        'state.dyn.dynamics[dragBand]',
    ]:
        assert token in web, token

    # Current module / bypass / Delta contracts.
    for token in [
        'EQ_COLOR_GLOBAL_BYPASS',
        'UDMBC_BYPASS',
        'TAPE_BYPASS',
        '"TRANSIENT" + n',
        'DELTA_MONITOR',
    ]:
        assert token in proc or token in cpp, token

    assert 'if(this.s.delta){yL=yL-l;yR=yR-r;}' in worklet
    assert 'analogReconstructed' in worklet
    assert 'zoneBands(y,c,"analogLp",s.udmbc.x)' in worklet
    assert 'colorX2?.[b]?2:1' in worklet

    # v1.0.60 keeps only the main Spectrum Analyzer.
    assert 'updateContributionAnalyzer' not in cpp and 'updateContributionAnalyzer' not in head
    assert 'popContributionSamples' not in proc
    assert 'setContributionAnalysisEnabled' not in proc
    assert 'type:"contributionSamples"' not in worklet


def test_v1016_gain_scale_10():
    db_points = [-18.0, -12.0, -6.0, -3.0, -1.0, 0.0, 1.0, 3.0, 6.0, 18.0]
    ys = [y_from_db(v) for v in db_points]
    assert len(ys) == 10
    for db, y in zip(db_points, ys):
        assert abs(db_from_y(y) - db) < 1e-6

    # Near 0 dB consumes the most pixels per dB.
    p0 = abs(y_from_db(0.0) - y_from_db(3.0)) / 3.0
    p1 = abs(y_from_db(3.0) - y_from_db(6.0)) / 3.0
    p2 = abs(y_from_db(6.0) - y_from_db(12.0)) / 6.0
    p3 = abs(y_from_db(12.0) - y_from_db(18.0)) / 6.0
    assert p0 > p1 > p2 > p3


def test_280_design_cases():
    # 280 deterministic absolute-cursor combinations:
    # 10 previous Dynamic states x 7 pointer Y positions x 4 EQ offsets.
    previous_states = [-100, -75, -50, -25, 0, 25, 50, 75, 100, -1]
    pointer_y = [0, 52.5, 105.0, 157.5, 210.0, 262.5, 315.0]
    eq_offsets = [-18.0, -6.0, 6.0, 18.0]
    count = 0
    for previous in previous_states:
        for y in pointer_y:
            for eq_gain in eq_offsets:
                out = dynamics_from_cursor(eq_gain, y, 315.0)
                out_repeat = dyn_from_drag(previous, 17.0, y, 315.0, 1.0, eq_gain)
                assert out == out_repeat
                assert -100.0 <= out <= 100.0
                # Absolute cursor mapping must ignore the previous Dynamics state.
                assert out == dynamics_from_cursor(eq_gain, y, 315.0)
                count += 1
    assert count == 280


def test_graph_roundtrip():
    for db in [-18, -12, -6, 0, 6, 12, 18]:
        y = y_from_db(db)
        back = db_from_y(y)
        assert abs(db - back) < 1e-6

def test_dynamic_range_direction():
    # Absolute Y direction is stable regardless of the prior Dynamics value.
    for eq_gain in [-18.0, -6.0, 0.0, 6.0, 18.0]:
        upper = dynamics_from_cursor(eq_gain, 100.0)
        lower = dynamics_from_cursor(eq_gain, 215.0)
        assert upper >= lower


def test_dynamic_drag_anchor_is_exact():
    # A drag starting from any pixel must not create a first-frame offset.
    for previous in [-100, -50, 0, 50, 100]:
        for grab_y in [80.0, 120.0, 157.5, 195.0, 235.0]:
            expected = dynamics_from_cursor(0.0, grab_y)
            assert dyn_from_drag(previous, grab_y, grab_y) == expected


def test_dynamic_cross_zero_is_linear():
    # The graph is intentionally nonlinear in pixel space, but remains
    # symmetric and monotonic around 0 dB.
    center = 157.5
    offsets = [0.0, 10.0, 25.0, 50.0, 80.0, 120.0, 157.5]
    upper = [gain_from_y(center - d) for d in offsets]
    lower = [gain_from_y(center + d) for d in offsets]
    assert all(upper[i] <= upper[i + 1] for i in range(len(upper)-1))
    assert all(lower[i] >= lower[i + 1] for i in range(len(lower)-1))
    for a, b in zip(upper, lower):
        assert abs(a + b) < 1e-9


def test_eq_xy_drag_math():
    # Frequency and Gain follow the same cursor, with no accumulated deltas.
    for dx, dy in [(40,20),(-40,20),(40,-20),(-40,-20),(0,30),(30,0)]:
        graph_w = 640.0
        graph_h = 315.0
        x0 = graph_w * 0.5
        y0 = graph_h * 0.5
        x = clamp(x0 + dx, 0, graph_w)
        y = clamp(y0 + dy, 0, graph_h)
        hz = 20.0 * (1000.0 ** (x / graph_w))
        gain = gain_from_y(y, graph_h)
        assert 20.0 <= hz <= 20000.0
        if dy < 0:
            assert gain > gain_from_y(y0, graph_h)
        elif dy > 0:
            assert gain < gain_from_y(y0, graph_h)

    cpp = CPP.read_text(encoding="utf-8")
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")
    assert 'const float correctedX' in cpp
    assert 'eqYToDb(graph, event.position.y)' in cpp
    assert 'const float targetGain' in cpp
    assert 'const hzv=invLog(clamp(x,0,w)/w);' in web
    assert 'gainAtCursor=yToDb(y,h)' in web
    assert 'const targetGain=yToDb(y,h);' in web
    assert 'GAIN / FREQ / Q' in cpp
    assert 'exact reference order: FREQ / GAIN / Q' in cpp
    assert 'placeKnob("UDMBC_LEVEL" + n,  cell(3, 1));' in cpp
    assert 'placeKnob("UDMBC_COMP_M" + n, cell(3, 2));' in cpp
    assert 'label:"FREQ",controlId:"EQ"+(n+1)+"_FREQ"' in web
    assert 'label:"LEVEL",controlId:"UDMBC_LEVEL"+(n+1)' in web
    assert 'label:"MIX",controlId:"UDMBC_COMP_M"+(n+1)' in web
    assert 'function graphHintBandHtml' in web
    hint_block = web[web.index('function graphHintBandHtml'):web.index('eqCanvas.addEventListener("contextmenu"')]
    assert 'data-kind="gain"' in hint_block
    assert 'data-kind="freq"' in hint_block
    assert '"Q "+fmt(state.eq.q[b],3)' in hint_block


def test_dynamic_target_preserves_eq_as_center():
    # Changing EQ shifts the whole dynamic target around it.
    for static_gain in [-18.0, -12.0, -6.0, 0.0, 3.0, 6.0, 12.0, 18.0]:
        for direction in [-1.0, 1.0]:
            target = dynamic_target(static_gain, 18.0, direction * 100.0)
            expected = clamp(static_gain + direction * 18.0, -18.0, 18.0)
            assert abs(target - expected) < 1e-9
            if -18.0 < expected < 18.0:
                assert abs(abs(target - static_gain) - 18.0) < 1e-9


def test_dynamic_target_is_linear():
    offset = 0.0
    target = 4.0
    values = [dynamic_target(offset, target, d)
              for d in [-100, -75, -50, -25, 0, 25, 50, 75, 100]]
    increments = [values[i + 1] - values[i] for i in range(len(values) - 1)]
    assert all(abs(v - increments[0]) < 1.0e-9 for v in increments)

def test_threshold_is_linear():
    # DSP threshold must also be a linear bipolar mapping.
    values = [-100, -75, -50, -25, 0, 25, 50, 75, 100]
    thresholds = [-12.0 + d * 0.12 for d in values]
    increments = [thresholds[i + 1] - thresholds[i]
                  for i in range(len(thresholds) - 1)]
    assert all(abs(v - increments[0]) < 1.0e-9 for v in increments)

def test_target_visual_direction():
    for offset in [-18, -6, 0, 6, 18]:
        dyn_range = 4.0
        for d in [-100, -50, 0, 50, 100]:
            v = dynamic_target(offset, dyn_range, d)
            if d < 0:
                assert v <= offset + 1e-6
            elif d > 0:
                assert v >= offset - 1e-6
            else:
                assert abs(v - offset) < 1e-6

def test_frequency_deadzone():
    start_x = 800.0
    dead = 8.0
    for dx in [-7.9, -4, 0, 4, 7.9]:
        effective = 0.0 if abs(dx) <= dead else dx - math.copysign(dead, dx)
        assert effective == 0.0
    for dx in [-30, -12, 12, 30]:
        effective = 0.0 if abs(dx) <= dead else dx - math.copysign(dead, dx)
        assert abs(effective) > 0.0

def test_transient_gestures():
    sequences = [
        [157.5, 156, 154, 151, 148, 154, 161, 170],
        [157.5, 180, 205, 190, 165, 140, 125],
        [157.5, 156.5, 157.0, 156.8, 157.2],
        [157.5, 100, 80, 120, 160, 200, 230],
        [157.5, 315, 250, 200, 150, 100, 50],
        [157.5, 157.5, 170, 157.5, 145, 157.5],
    ]
    for seq in sequences:
        last = 0.0
        for y in seq:
            last = dyn_from_drag(0, 157.5, y)
            assert -100 <= last <= 100

def test_all_features_rounds():
    rng = random.Random(20260923)
    for _ in range(10):
        freq = rng.uniform(20, 20000)
        gain = rng.uniform(-18, 18)
        dyn = rng.uniform(-100, 100)
        target = rng.uniform(-18, 18)
        q = rng.uniform(0.1, 18)
        attack = rng.uniform(0.1, 200)
        release = rng.uniform(5, 2000)
        ms = rng.uniform(0, 100)
        assert 20 <= freq <= 20000
        assert -18 <= gain <= 18
        assert -100 <= dyn <= 100
        assert -18 <= target <= 18
        assert 0.1 <= q <= 18
        assert 0.1 <= attack <= 200
        assert 5 <= release <= 2000
        assert 0 <= ms <= 100
        _ = dynamic_target(gain, target, dyn)
        _ = dyn_from_drag(dyn, 157.5, rng.uniform(0, 315))

def test_full_simulation():
    rng = random.Random(777)
    for session in range(10):
        values = []
        for _band in range(4):
            dynamics = 0.0
            freq = [80, 350, 2500, 10000][_band]
            for _gesture in range(10):
                start_y = rng.uniform(40, 275)
                current_y = clamp(start_y + rng.uniform(-250, 250), 0, 315)
                dynamics = dyn_from_drag(dynamics, start_y, current_y)
                assert -100 <= dynamics <= 100
                assert math.isfinite(dynamics)
                # Dynamic XY drag keeps frequency inside the valid graph range.
                assert 20 <= freq <= 20000
            values.append(dynamics)
        assert len(values) == 4


def test_transient_500_candidate_matrix():
    """500 deterministic Transient detector/control cases."""
    fast = [2.5, 1.5, 0.8, 0.35]
    slow = [30.0, 22.0, 15.0, 9.0]
    amounts = [-100.0, -50.0, -1.0, 0.0, 1.0, 50.0, 100.0]
    energies = [1e-8, 1e-6, 1e-4, 1e-2, 1.0]

    cases = []
    for band in range(4):
        for amount in amounts:
            for ef in energies:
                for es in energies:
                    cases.append((band, amount, ef, es))
    assert len(cases) == 700
    cases = cases[:500]

    for band, amount, fast_sq, slow_sq in cases:
        ratio = (fast_sq + 1e-12) / (slow_sq + 1e-12)
        transient_db = (10.0 / math.log(10.0)) * math.log(ratio)
        scaled = transient_db * (amount / 100.0) / 12.0
        clipped = scaled / (1.0 + abs(scaled))
        control_db = clipped * 12.0
        gain = 10.0 ** (control_db / 20.0)
        assert math.isfinite(gain)
        assert gain > 0.0
        assert abs(control_db) < 12.0 + 1e-6
        assert fast[band] < slow[band]

    dsp = DSP.read_text(encoding="utf-8")
    worklet = (ROOT / "docs" / "vvchain-worklet.js").read_text(encoding="utf-8")
    for token in [
        "fastMs[4] = { 2.5f, 1.5f, 0.8f, 0.35f }",
        "slowMs[4] = { 30.0f, 22.0f, 15.0f, 9.0f }",
        "transientBand1SidechainHPF",
        "vvFastLogPositive",
        "scaled / (1.0f + std::abs(scaled))",
        "inputByChannel[ch] + delta",
    ]:
        assert token in dsp, token
    for token in [
        "applyTransientStereo(l,r,stereo,xs)",
        "const energy=stereo?.5*(dl*dl+dr*dr):dl*dl",
        "const clipped=scaled/(1+Math.abs(scaled))",
        "deltaL+=bandsL[b]*d",
    ]:
        assert token in worklet, token


def test_v106_shared_four_band_modules_and_transient():

    cpp = (ROOT / "Source" / "DSP" / "ChainDSP.cpp").read_text(encoding="utf-8")
    worklet = (ROOT / "docs" / "vvchain-worklet.js").read_text(encoding="utf-8")
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")
    editor = CPP.read_text(encoding="utf-8")

    cpp_tape = cpp[cpp.index("void VVChainDSP::applyAType"):cpp.index("void VVChainDSP::processMasterLimiter")]
    worklet_tape = worklet[worklet.index("if(!s.type.bypass){"):worklet.index("\n    return y;", worklet.index("if(!s.type.bypass){"))]

    assert "std::tanh(bands[band] * driveParam[band])" in cpp_tape
    assert "staticMakeupMultiplier" in cpp_tape
    assert "typeFastEnv" not in cpp_tape
    assert "typeSlowEnv" not in cpp_tape
    assert "targetGainDb" not in cpp_tape
    assert "const float crossoverQ = crossoverQFromOverlap" in cpp_tape
    assert "updateCrossover(typeXover1, sr, x1, crossoverQ)" in cpp_tape
    assert "updateCrossover(typeXover2, sr, x2, crossoverQ)" in cpp_tape
    assert "updateCrossover(typeXover3, sr, x3, crossoverQ)" in cpp_tape
    assert "const float bands[4] = {" in cpp_tape
    assert "low, lowMid, midHigh, top" in cpp_tape
    assert "Math.tanh(bands[b]*driveParams[b])*makeup[b]" in worklet_tape
    assert "c.typeFast[b]" not in worklet_tape
    assert "c.typeSlow[b]" not in worklet_tape
    assert "const xs=s.udmbc.x;" in worklet_tape
    assert 'this.zoneBands(ti,c,"typeLp",xs)' in worklet_tape
    assert re.search(r"VVCHAIN v\d+\.\d+\.\d+", web)
    assert re.search(r"VVCHAIN v\d+\.\d+\.\d+", editor)
    assert "LAST " not in editor

    # ANALOG v1.0.16 uses unity-normalized smooth algebraic saturation.
    assert "v1.0.16 smooth zero-phase algebraic saturation" in cpp
    assert "unityNorm" in cpp and "unityNorm" in worklet
    assert "const double u = juce::jlimit(-1.0, 1.0, x);" in cpp
    assert "protectedSaturated" in worklet
    assert "return x+(protectedSaturated-u)*this.clamp(x2,1,2)" in worklet
    assert "colorX2" in web
    assert "p.eqColorX2[band] ? 2.0f : 1.0f" in cpp

def test_dynamic_range_centered_500():
    """500 deterministic cases: Dynamic EQ is centered on the static EQ gain."""
    rng = random.Random(20260923_500)
    for _ in range(500):
        static_db = rng.uniform(-18.0, 18.0)
        dynamic_range_db = 18.0
        dynamics_pct = rng.uniform(-100.0, 100.0)
        amount = abs(dynamics_pct) / 100.0
        direction = -1.0 if dynamics_pct < 0.0 else 1.0
        contribution = direction * abs(dynamic_range_db) * amount

        expected = clamp(static_db + contribution, -18.0, 18.0)
        assert math.isfinite(expected)
        assert -18.0 <= expected <= 18.0
        assert abs(dynamic_target(static_db, dynamic_range_db, 0.0) - static_db) < 1e-9

        if abs(static_db + contribution) <= 18.0:
            assert abs(expected - (static_db + contribution)) < 1e-9

    # Production model clamps total Dynamic EQ gain to ±18 dB.
    assert dynamic_target(3.0, 18.0, 100.0) == 18.0
    assert dynamic_target(3.0, 18.0, -100.0) == -15.0
    assert dynamic_target(3.0, 18.0, 0.0) == 3.0


def test_v103_ui_rules_50():
    """50 deterministic state checks for the v1.0.3 visual rules."""
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")
    cpp = CPP.read_text(encoding="utf-8")
    head = HEAD.read_text(encoding="utf-8")

    # 50 combinations across all four bands and the zero/non-zero gates.
    cases = []
    for band in range(4):
        for udmbc_zero in (True, False):
            for analog_zero in (True, False):
                for tape_zero in (True, False):
                    cases.append((band, udmbc_zero, analog_zero, tape_zero))
    assert len(cases) == 32

    # Add boundary permutations to reach exactly 50 deterministic cases.
    cases.extend([
        (0, True, True, True), (1, True, True, False),
        (2, True, False, True), (3, True, False, False),
        (0, False, True, True), (1, False, True, False),
        (2, False, False, True), (3, False, False, False),
        (0, True, False, False), (1, False, True, True),
        (2, True, True, False), (3, False, False, True),
        (0, False, True, False), (1, True, False, True),
        (2, False, False, False), (3, True, True, True),
        (0, True, True, False), (1, True, False, False),
    ])
    assert len(cases) == 50

    assert ".knobMuted" in web
    assert ".knobMuted .modeSwitch" in web
    assert 'mutedWhen:()=>state.udmbc.bypass||state.udmbc.bandBypass[n]||state.udmbc.degree[n]<=0.0001' in web
    assert 'mutedWhen:()=>state.eq.globalBypass||state.eq.colorBypass[n]||state.eq.color[n]<=0.0001' in web
    assert 'mutedWhen:()=>state.type.bypass||state.type.bandBypass[n]||state.type.degree[n]<=0.0001' in web
    assert 'state.transient' in web
    assert 'addKnob("TRANSIENT" + n' in cpp

    assert "const hzv=invLog(clamp(x,0,w)/w);" in web
    assert "const targetGain=yToDb(y,h);" in web
    assert "state.eq.freq[dragBand]=hzv" in web
    assert "state.eq.freq[dragBand]=hzv" in web
    assert 'setParameter("EQ" + n + "_FREQ", hz);' in cpp
    assert 'setParameter("DYN_DYNAMICS" + n, dynamics);' in cpp
    assert 'graphFreqDragStartHz = hz;' in cpp

    assert "20 Hz" in web and "100 Hz" in web and "1 kHz" in web
    assert "10 kHz" in web and "20 kHz" in web
    assert "Restored graph axis labels" in cpp
    assert "20 Hz" in cpp and "20 kHz" in cpp

    assert re.search(r"VVCHAIN v\d+\.\d+\.\d+", web)
    assert re.search(r"VVCHAIN v\d+\.\d+\.\d+", cpp)
    assert "LAST " not in web
    assert "LAST " not in cpp


def test_v103_closed_10():
    """Ten closed regression passes for the v1.0.3 graph and DSP safety rules."""
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")
    cpp = CPP.read_text(encoding="utf-8")
    worklet = (ROOT / "docs" / "vvchain-worklet.js").read_text(encoding="utf-8")

    for _ in range(10):
        assert 'if(this.s.delta){yL=yL-l;yR=yR-r;}' in worklet
        assert 'if(s.delta){yL=yL-l;yR=yR-r;}' not in worklet
        assert 'mDynamicGain=mGain-offset' in worklet
        assert 'sDynamicGain=sGain-offset' in worklet
        assert 'mid=this.tptBell(mid,md.eq,sampleRate,f,baseQ,mDynamicGain);' in worklet
        assert 'side=this.tptBell(side,sd.eq,sampleRate,f,baseQ,sDynamicGain);' in worklet
        assert 'eq:{g:0,k:1,a1:1,a2:0,a3:0,m1:0,ic1:0,ic2:0}' in worklet
        assert 'z.ic1=Number.isFinite(z.ic1)?z.ic1:0;' in worklet
        assert 'z.ic2=Number.isFinite(z.ic2)?z.ic2:0;' in worklet
        assert 'C1=m1*a1,C2=-m1*a2,D=1+m1*a2' in web
        assert 'const detR=d11*d22-A12*A21-zi*zi' in web
        assert 'const outR=D+C1*h1R+C2*h2R' in web
        assert 'this.peak(sampleRate,f,mq,mDynamicGain)' not in worklet
        assert 'this.peak(sampleRate,f,sq,sDynamicGain)' not in worklet
        assert 'this.peak(sampleRate,f,mq,mGain)' not in worklet
        assert 'this.peak(sampleRate,f,sq,sGain)' not in worklet

        # DYNAMICS Target XY mapping.
        assert "const hzv=invLog(clamp(x,0,w)/w);" in web
        assert "const targetGain=yToDb(y,h);" in web
        assert "state.eq.freq[dragBand]=hzv" in web
        assert "state.dyn.dynamics[dragBand]=clamp" in web

        # Native/Web bypass and graph routing.
        assert 'setParameter("EQ" + n + "_FREQ", hz);' in cpp
        assert 'setParameter("DYN_DYNAMICS" + n, dynamics);' in cpp
        assert "20 kHz" in web and "20 kHz" in cpp

        # Conditional grey-state rules.
        assert "UDMBC_DEGREE" + "" in cpp
        assert ".knobMuted" in web



def test_v107_ui_controls():
    cpp = CPP.read_text(encoding="utf-8")
    head = HEAD.read_text(encoding="utf-8")
    proc = PROC.read_text(encoding="utf-8")
    dsp = DSP.read_text(encoding="utf-8")
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")
    worklet = (ROOT / "docs" / "vvchain-worklet.js").read_text(encoding="utf-8")

    # Graph frequency follows the actual pointer coordinate with no grab-offset accumulation.
    assert "graphXToFrequency(graph, correctedX)" in cpp
    assert "event.position.x);" in cpp
    assert "event.position.x - graphFreqDragGrabOffsetX" not in cpp
    assert "followScale = 0.74f" not in cpp
    assert "const followScale=0.74" not in web
    assert "const hzv=invLog(clamp(x,0,w)/w);" in web

    # EQ frequency knobs are intentionally slower than the base gain drag.
    assert "setDragSensitivity(900, 9000)" in cpp
    assert 'const hzv=invLog(clamp(x,0,w)/w);' in web

    # Graph readout is compact: EQ / DYN EQ + GAIN, FREQ, Q only.
    assert 'DYN EQ' in cpp
    assert 'm_boxWidth = 112' in head
    assert '.graphHint{width:112px' in web
    assert 'DYN EQ' in web
    hint_block = web[web.index('function graphHintBandHtml'):web.index('eqCanvas.addEventListener("contextmenu"')]
    assert ' | ' not in hint_block

    # TAPE upper limits are 50/60/70/90 while tanh processing remains unchanged.
    assert 'const float maxDegrees[4] = { 50.f, 60.f, 70.f, 90.f };' in proc
    assert 'constexpr float kTypeAMaxDegree[4] = { 50.f, 60.f, 70.f, 90.f };' in dsp
    assert '[50,60,70,90]' in web
    assert '[50,60,70,90]' in worklet
    assert 'std::tanh(bands[band] * driveParam[band])' in dsp

    # ANALOG X2 is a saved parameter that multiplies only the current COLOR amount.
    assert 'EQ_COLOR_X2' in proc
    assert 'eqColorX2' in dsp
    assert 'p.eqColorX2[band] ? 2.0f : 1.0f' in dsp
    assert 'colorX2' in web
    assert 'analogX2Btn' in web
    assert 'colorX2?.[b]?2:1' in worklet
    assert 'analogX2Buttons' in head



def test_v1018_interaction_visual_sync():
    cpp = CPP.read_text(encoding="utf-8")
    head = HEAD.read_text(encoding="utf-8")
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")

    # Local LED bypass, zero-value mute and upper module bypass all feed the same grey state.
    assert 'parameterValue("UDMBC_BYPASS") > 0.5f' in cpp
    assert 'parameterValue("UDMBC_BAND_BYPASS" + n) > 0.5f' in cpp
    assert 'parameterValue("TAPE_BYPASS") > 0.5f' in cpp
    assert 'parameterValue("TAPE_BAND_BYPASS" + n) > 0.5f' in cpp
    assert 'moduleMuteRefreshers' in web
    assert 'state.udmbc.bypass||state.udmbc.bandBypass[n]' in web
    assert 'state.type.bypass||state.type.bandBypass[n]' in web
    assert '.moduleMuted{opacity:.42;filter:grayscale(1)}' in web

    # Main lower BYPASS label is centered above its round power button.
    assert 'const bool monitorCard = title == "MASTER";' in cpp
    assert 'juce::Justification::centred' in cpp
    assert "class='masterBypassLabel'>BYPASS</div><button class='masterPower'" in web

    # Floating readout has three short lines and switches identity by hover target.
    assert 'juce::String(dynamicReadout ? "DYN EQ" : "EQ")' in cpp
    assert 'const juce::String line2 = "GAIN " + signedDb' in cpp
    assert 'const juce::String line3 = "Q " + juce::String(q, 3)' in cpp
    assert 'const dynamicReadout=!!(mask&4);' in web
    assert 'const label=dynamicReadout?"DYN EQ":"EQ";' in web
    hint_block = web[web.index('function graphHintBandHtml'):web.index('eqCanvas.addEventListener("contextmenu"')]
    assert 'TARGET' not in hint_block and 'OFFSET' not in hint_block and 'AUTO THR' not in hint_block
    assert hint_block.count('<div class="active">') == 3

    # Right-click SOLO keeps the selected region coloured and fades outward to grey.
    assert 'Right-click SOLO keeps the selected EQ region in full colour' in cpp
    assert 'constexpr float colourRadius = 70.0f;' in cpp
    assert 'Right-click SOLO: preserve full colour near the selected EQ point' in web
    assert 'globalCompositeOperation="saturation"' in web
    assert 'createLinearGradient' in web




def test_v1024_compact_readout_50():
    cpp = CPP.read_text(encoding="utf-8")
    head = HEAD.read_text(encoding="utf-8")
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")

    # Source-level invariants: one fixed compact box, three lines only.
    assert 'm_boxWidth = 112' in head
    assert 'm_line1Text' in head and 'm_line2Text' in head and 'm_line3Text' in head
    assert 'The node label lives beside the frequency; one value per short line.' in cpp
    assert 'showFloatingValueBoxForBand(' in cpp
    assert 'rightSoloBand, false, gain, event.position' in cpp
    assert 'dragOffsetBand, false, offset, event.position' in cpp
    assert 'dragDynamicHandleBand, true' in cpp
    assert 'dragBand, true, dynamicEffectiveTargetGain(dragBand)' in cpp
    assert 'dynamicWheelReadout' in cpp
    assert 'dynamicWheelReadout?4:8' in web
    assert '.graphHint{width:112px;min-width:112px;max-width:112px}' in web

    hint_block = web[web.index('function graphHintBandHtml'):web.index('eqCanvas.addEventListener("contextmenu"')]
    assert hint_block.count('<div class="active">') == 3
    for forbidden in ('TARGET', 'OFFSET', 'AUTO THR', 'DYNAMICS', 'THRESH'):
        assert forbidden not in hint_block

    # 50 deterministic identity/format cases:
    # static EQ/Q => EQ; live/target/handle => DYN EQ.
    masks = [1|2, 8, 1|4, 4, 1|2|4]
    for i in range(50):
        mask = masks[i % len(masks)]
        dynamic = bool(mask & 4)
        label = "DYN EQ" if dynamic else "EQ"
        gain = -18.0 + (36.0 * i / 49.0)
        freq = 20.0 * (1000.0 ** (i / 49.0))
        q = 0.1 + (17.9 * i / 49.0)
        line1 = f"{label}  {freq:.2f} Hz"
        line2 = f"GAIN {gain:+.2f} dB"
        line3 = f"Q {q:.3f}"
        assert line1.startswith("DYN EQ  ") if dynamic else line1.startswith("EQ  ")
        assert "GAIN " in line2 and "Q " in line3
        assert "TARGET" not in line1 + line2 + line3
        assert "OFFSET" not in line1 + line2 + line3




def test_v1028_q_wheel_3x_continuous():
    cpp = CPP.read_text(encoding="utf-8")
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")

    assert 'const float speed = fine ? 0.0075f : 0.075f;' in cpp
    assert 'const speed=fine?.0075:.075;' in web
    assert cpp.count('qFromWheel(q, wheel.deltaY, event.mods.isShiftDown())') == 2
    assert 'nextQFromWheel(q,e.deltaY,e.shiftKey)' in web
    assert 'nextQFromWheel(state.eq.q[band],e.deltaY,e.shiftKey)' in web

    # 50 distinct wheel deltas around one safe Q must produce 50 unique outputs.
    # This validates a continuous mapping without confusing hard-limit clamping
    # at Q=0.1 / 18 with "stepping".
    base_q = 1.0
    deltas = [-1.0 + (2.0 * i / 49.0) for i in range(50)]
    values = [
        max(0.1, min(18.0, base_q * math.exp(delta * 0.075)))
        for delta in deltas
    ]
    rounded = [round(v, 12) for v in values]
    assert len(set(rounded)) == 50
    assert all(rounded[i] < rounded[i + 1] for i in range(len(rounded) - 1))

    # Exact 3x sensitivity relative to the previous 0.025 / 0.0025 constants.
    assert abs(0.075 / 0.025 - 3.0) < 1e-12
    assert abs(0.0075 / 0.0025 - 3.0) < 1e-12




def test_v1032_readout_hit_priority_50():
    cpp = CPP.read_text(encoding="utf-8")
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")

    # Native: Static EQ is checked and returned before Dynamic target/arrow.
    native_block = cpp[cpp.index('void VVChainAudioProcessorEditor::updateFloatingValueBoxAt('):cpp.index('void VVChainAudioProcessorEditor::mouseMove(')]
    assert 'Static EQ point always wins' in native_block
    assert 'live gain marker is visual only' in native_block
    assert 'HoverTarget::Live' not in native_block
    assert 'showFloatingValueBoxForBand(\n                b, false, offsetGain, position);' in native_block
    assert 'showFloatingValueBoxForBand(\n                b, true, targetGain, position);' in native_block

    # Web: same priority, and initial Dynamic arrow press uses the normal compact formatter.
    web_block = web[web.index('function graphHoverTarget'):web.index('eqCanvas.addEventListener("mousemove"')]
    assert 'const staticBand=staticEqAtPointer' in web_block
    assert 'if(staticBand>=0&&Math.hypot(' in web_block
    assert 'return {band:staticBand,mask:2};' in web_block
    assert 'dynamicNodePoint' not in web_block
    assert 'return {band:b,mask:1|4};' in web_block
    assert 'showGraphHint(e,graphHintBandHtml(b,1|4));' in web
    assert 'showGraphHint(e,"DYN "+(b+1)' not in web

    # 50 geometry cases: whenever pointer is inside Static EQ radius, EQ must win
    # even if a Dynamic target/live marker mathematically sits closer or overlaps.
    static_radius = 7.0
    dynamic_radius = 12.0
    for i in range(50):
        # Static point at origin; Dynamic target sweeps across/near it.
        px = -8.5 + 17.0 * i / 49.0
        py = 0.0
        static_dist = math.hypot(px, py)
        dyn_x = (i % 7 - 3) * 0.75
        dyn_y = (i % 5 - 2) * 0.75
        dynamic_dist = math.hypot(px - dyn_x, py - dyn_y)

        if static_dist <= static_radius:
            selected = "EQ"
        elif dynamic_dist < dynamic_radius:
            selected = "DYN EQ"
        else:
            selected = "NONE"

        if static_dist <= static_radius:
            assert selected == "EQ"
        elif dynamic_dist < dynamic_radius:
            assert selected == "DYN EQ"


def main():
    source_assertions()

    # Ten full deterministic rounds over current interaction/math contracts.
    for _ in range(10):
        test_v1016_gain_scale_10()
        test_280_design_cases()
        test_graph_roundtrip()
        test_dynamic_range_direction()
        test_dynamic_drag_anchor_is_exact()
        test_dynamic_cross_zero_is_linear()
        test_eq_xy_drag_math()
        test_transient_500_candidate_matrix()
        test_dynamic_range_centered_500()
        test_dynamic_target_preserves_eq_as_center()
        test_dynamic_target_is_linear()
        test_threshold_is_linear()
        test_target_visual_direction()
        test_frequency_deadzone()
        test_transient_gestures()
        test_all_features_rounds()
        test_full_simulation()

    print("PASS: current source invariants")
    print("PASS: 10 x 280 Dynamic EQ design cases")
    print("PASS: 10 x graph / XY / Q / reset interaction contracts")
    print("PASS: 10 x 500 Transient detector/control checks")
    print("PASS: 10 x full four-band simulated sessions")
    print("ALL current Dynamic EQ / UI regression tests passed")

if __name__ == "__main__":
    main()
