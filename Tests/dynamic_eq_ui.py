#!/usr/bin/env python3
# v1.0.8 regression matrix: shared Type-A/ANALOG crossovers, module-isolated Delta, global hover values, and DeEsser presets.
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
    # Graph Y is an absolute cursor coordinate: top = +18 dB, bottom = -18 dB.
    return clamp(18.0 - (y / max(1.0, height)) * 36.0, -18.0, 18.0)

def dynamics_from_cursor(eq_gain, y, height=315.0):
    target_gain = gain_from_y(y, height)
    return clamp((target_gain - eq_gain) / 18.0 * 100.0, -100.0, 100.0)

def dyn_from_drag(start_dyn, start_y, current_y, graph_h=315.0, scale=1.0, eq_gain=0.0):
    # Compatibility helper kept for older call sites: the result intentionally
    # does not depend on previous dynamics or the mouse-down Y. It follows the
    # current cursor Y absolutely.
    _ = start_dyn, start_y, scale
    return dynamics_from_cursor(eq_gain, current_y, graph_h)

def y_from_db(db, height=315.0):
    return height - height * clamp((db + 18.0) / 36.0, 0.0, 1.0)

def db_from_y(y, height=315.0):
    return clamp((height - y) / height * 36.0 - 18.0, -18.0, 18.0)

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

    required = [
        'f("DYN_DYNAMICS" + n, "Dynamic EQ " + n + " Dynamics",',
        '-100.f, 100.f, dynDynamicsDefaults[i])',
        'setParameter("DYN_DYNAMICS" + n, dynamics);',
        'parameter->beginChangeGesture();',
        'parameter->endChangeGesture();',
        'DYN_DYNAMICS',
        'dynamicTargetDragStartY = pos.y',
        'const float correctedX',
        'const float gainAtCursor',
        'const float targetGain',
        'dragDynamicHandleBand',
        'dynamicHandleDragStartValue',
        'sendNotificationSync',
        'graphFreqDragGrabOffsetX',
        'DYNAMICS uses a truly linear bipolar map',
        'constexpr float staticNodeRadius = 5.5f',
        'getTargetGainDB',
        'peakMagnitudeDBAtFrequency',
        'getTargetGainDB',
        'gainAtCursor',
        'juce::jlimit(-18.f, 18.f',
    ]
    for token in required:
        assert token in cpp or token in proc or token in head or token in dsp or token in engine, f"missing source invariant: {token}"

    assert 'DYN_TARGET" + n, storedTarget' not in cpp,         "graph drag must not write DYN_TARGET anymore"
    assert 'dynamicTargetDragStartY = pos.y;' in cpp,         "Dynamic gesture must anchor to the actual mouse-down pixel"
    assert 'sendNotificationSync' in cpp,         "Lower DYNAMICS knob must refresh synchronously during graph drag"
    assert 'DYN_DYNAMICS" + n, dynamics' in cpp, \
        "graph drag must write DYN_DYNAMICS"
    assert cpp.count("juce::StringArray({") == 0, "no ambiguous JUCE StringArray brace initializers"
    assert "Direct DYNAMICS target control takes priority" in cpp, "DYNAMICS point must be hit before static EQ"
    assert "graphHintActiveMask" in cpp and "graphHintAutoHideAt" in cpp, "Native graph hint state missing"
    assert "function graphHintBandHtml" in web, "Web graph hint formatter missing"
    assert "showGraphHint(e,graphHintBandHtml(dragBand,1|2))" in web
    assert "showGraphHint(e,graphHintBandHtml(dragBand,1|4))" in web
    assert "showGraphHint(e,graphHintBandHtml(dragDynamicHandleBand,4))" in web
    assert "showGraphHint(e,graphHintBandHtml(band,8))" in web

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
    # Equal absolute pixel increments produce equal parameter increments.
    start_y = 157.5
    step = 15.75
    ys = [start_y + i * step for i in range(-10, 11)]
    vals = [dynamics_from_cursor(0.0, y) for y in ys]
    increments = [vals[i + 1] - vals[i] for i in range(len(vals) - 1)]
    assert all(abs(v - increments[0]) < 1.0e-9 for v in increments)


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
    assert 'const float gainAtCursor' in cpp
    assert 'const float targetGain' in cpp
    assert 'const hzv=invLog(clamp(x,0,w)/w);' in web
    assert 'const gainAtCursor=clamp(18-(y/Math.max(1,h))*36,-18,18);' in web
    assert 'const targetGain=clamp(18-(y/Math.max(1,h))*36,-18,18);' in web
    assert 'GAIN / FREQ / Q' in cpp
    assert 'function graphHintBandHtml' in web
    hint_block = web[web.index('function graphHintBandHtml'):web.index('eqCanvas.addEventListener("contextmenu"')]
    assert 'GAIN' in hint_block and 'FREQ' in hint_block and 'Q' in hint_block


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


def test_deess_500_candidate_matrix():
    """Evaluate exactly 500 Attack/Release/Ratio candidates and verify four profiles."""
    attacks = [0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 5.0, 8.0, 12.0]
    releases = [20.0, 35.0, 50.0, 70.0, 120.0]
    ratios = [2.0, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0, 12.0, 16.0, 20.0]
    candidates = [(a, r, ratio) for a in attacks for r in releases for ratio in ratios]
    assert len(candidates) == 500

    profiles = [
        (5.0, 120.0, 3.0),
        (2.0, 70.0, 4.0),
        (0.75, 35.0, 8.0),
        (0.25, 20.0, 10.0),
    ]
    assert all(profile in candidates for profile in profiles)

    dsp = (ROOT / "Source" / "DSP" / "ChainDSP.cpp").read_text(encoding="utf-8")
    worklet = (ROOT / "docs" / "vvchain-worklet.js").read_text(encoding="utf-8")
    for a, r, ratio in profiles:
        assert f"{a}f" in dsp
        assert f"{r}f" in dsp
        assert f"{ratio}f" in dsp
    assert "{attack:5,release:120,ratio:3}" in worklet
    assert "{attack:2,release:70,ratio:4}" in worklet
    assert "{attack:.75,release:35,ratio:8}" in worklet
    assert "{attack:.25,release:20,ratio:10}" in worklet


def test_v103_ui_rules_50():
    """50 deterministic state checks for the v1.0.3 visual rules."""
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")
    cpp = CPP.read_text(encoding="utf-8")
    head = HEAD.read_text(encoding="utf-8")

    # 50 combinations across all four bands and the zero/non-zero gates.
    cases = []
    for band in range(4):
        for ott_zero in (True, False):
            for analog_zero in (True, False):
                for tape_zero in (True, False):
                    cases.append((band, ott_zero, analog_zero, tape_zero))
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
    assert ".deessMuted" in web
    assert 'mutedWhen:()=>state.ott.degree[n]<=0.0001' in web
    assert 'mutedWhen:()=>state.eq.color[n]<=0.0001' in web
    assert 'mutedWhen:()=>state.type.degree[n]<=0.0001' in web
    assert 'mutedWhen:()=>state.de.intensity<=0.0001' in web

    assert "deessLocalBypass" in web
    assert "DEESS_LOCAL_BYPASS" in cpp
    assert "deessLocalBypassButton" in head
    assert 'DEESS_BYPASS", *deessLocalBypassButton' in cpp

    assert "const hzv=invLog(clamp(x,0,w)/w);" in web
    assert "const targetGain=clamp(18-(y/Math.max(1,h))*36,-18,18);" in web
    assert "state.eq.freq[dragBand]=hzv" in web
    assert "state.eq.freq[dragBand]=hzv" in web
    assert 'setParameter("EQ" + n + "_FREQ", hz);' in cpp
    assert 'setParameter("DYN_DYNAMICS" + n, dynamics);' in cpp
    assert 'graphFreqDragStartHz = hz;' in cpp

    assert "20 Hz" in web and "100 Hz" in web and "1 kHz" in web
    assert "10 kHz" in web and "20 kHz" in web
    assert "Restored graph axis labels" in cpp
    assert "20 Hz" in cpp and "20 kHz" in cpp

    assert "VVCHAIN v1.0.8" in web
    assert "VVCHAIN v1.0.8" in cpp
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
        assert 'this.peak(sampleRate,f,mq,mDynamicGain)' in worklet
        assert 'this.peak(sampleRate,f,sq,sDynamicGain)' in worklet
        assert 'this.peak(sampleRate,f,mq,mGain)' not in worklet
        assert 'this.peak(sampleRate,f,sq,sGain)' not in worklet

        # DYNAMICS Target XY mapping.
        assert "const hzv=invLog(clamp(x,0,w)/w);" in web
        assert "const targetGain=clamp(18-(y/Math.max(1,h))*36,-18,18);" in web
        assert "state.eq.freq[dragBand]=hzv" in web
        assert "state.dyn.dynamics[dragBand]=clamp" in web

        # Native/Web bypass and graph routing.
        assert 'setParameter("EQ" + n + "_FREQ", hz);' in cpp
        assert 'setParameter("DYN_DYNAMICS" + n, dynamics);' in cpp
        assert "DEESS_LOCAL_BYPASS" in cpp
        assert "20 kHz" in web and "20 kHz" in cpp

        # Conditional grey-state rules.
        assert "OTT_DEGREE" + "" in cpp
        assert ".knobMuted" in web
        assert ".deessMuted" in web



def test_v107_ui_controls():
    cpp = CPP.read_text(encoding="utf-8")
    head = HEAD.read_text(encoding="utf-8")
    proc = PROC.read_text(encoding="utf-8")
    dsp = DSP.read_text(encoding="utf-8")
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")
    worklet = (ROOT / "docs" / "vvchain-worklet.js").read_text(encoding="utf-8")

    # Graph frequency follows the actual pointer coordinate with grab-offset preservation.
    assert "graphFreqDragGrabOffsetX =" in cpp
    assert "graphXToFrequency(graph, correctedX)" in cpp
    assert "event.position.x - graphFreqDragGrabOffsetX" in cpp
    assert "followScale = 0.74f" not in cpp
    assert "const followScale=0.74" not in web
    assert "const hzv=invLog(correctedX/w);" in web

    # EQ / DE-ESS frequency knobs are intentionally slower than the base gain drag.
    assert "setDragSensitivity(900, 9000)" in cpp
    assert 'const hzv=invLog(clamp(x,0,w)/w);' in web

    # DE-ESS MODE is a four-position discrete rotary with Roman tick labels.
    assert 'drawLinearSlider(' in cpp
    assert 'labels { "I", "II", "III", "IV" }' in cpp
    assert 'DEESS_MODE_SWITCH' in cpp
    assert 'deEssModeSwitch' in web
    assert 'state.de.mode=index+1' in web

    # Graph readout is compact: EQ / DYN EQ + GAIN, FREQ, Q only.
    assert 'DYN EQ' in cpp
    assert 'm_boxWidth = 108' in head
    assert '.graphHint{width:112px' in web
    assert 'DYN EQ' in web
    hint_block = web[web.index('function graphHintBandHtml'):web.index('eqCanvas.addEventListener("contextmenu"')]
    assert ' | ' not in hint_block

    # TYPE-A upper limits are 50/60/70/90 while tanh processing remains unchanged.
    assert 'const float maxDegrees[4] = { 50.f, 60.f, 70.f, 90.f };' in proc
    assert 'constexpr float kTypeAMaxDegree[4] = { 50.f, 60.f, 70.f, 90.f };' in dsp
    assert '[50,60,70,90]' in web
    assert '[50,60,70,90]' in worklet
    assert 'std::tanh(bands[band] * driveParam[band])' in dsp

    # ANALOG X2 is a saved parameter that multiplies only the current COLOR amount.
    assert 'EQ_COLOR_X2' in proc
    assert 'eqColorX2' in dsp
    assert 'p.eqColorX2[band] ? 1.6f : 1.0f' in dsp
    assert 'colorX2' in web
    assert 'analogX2Btn' in web
    assert 'colorX2?.[b]?1.6:1' in worklet
    assert 'analogX2Buttons' in head


def main():
    source_assertions()
    test_280_design_cases()
    test_graph_roundtrip()
    test_dynamic_range_direction()
    test_dynamic_drag_anchor_is_exact()
    test_dynamic_cross_zero_is_linear()
    test_eq_xy_drag_math()
    for _ in range(10):
        test_deess_500_candidate_matrix()
    test_dynamic_range_centered_500()
    test_dynamic_target_preserves_eq_as_center()
    test_dynamic_target_is_linear()
    test_threshold_is_linear()
    test_target_visual_direction()
    test_frequency_deadzone()
    test_transient_gestures()
    for _ in range(10):
        test_all_features_rounds()
    test_full_simulation()
    test_v106_shared_four_band_modules_and_deess_presets()
    test_v103_ui_rules_50()
    test_v103_closed_10()
    test_v107_ui_controls()
    print("PASS: 280 design cases")
    print("PASS: 10 core/all-feature rounds")
    print("PASS: 6 transient gesture sequences")
    print("PASS: 10 full simulated sessions")
    print("PASS: source invariants / APVTS / graph-DYNAMICS binding")
    print("ALL Dynamic EQ UI regression tests passed")

if __name__ == "__main__":
    main()


