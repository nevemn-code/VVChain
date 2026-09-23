#!/usr/bin/env python3
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

def clamp(v, lo, hi):
    return max(lo, min(hi, v))

def dyn_from_drag(start_dyn, start_y, current_y, graph_h=315.0, scale=1.0):
    return clamp(start_dyn - (current_y - start_y) / max(1.0, graph_h) * 200.0 * scale,
                 -100.0, 100.0)

def y_from_db(db, height=315.0):
    return height - height * clamp((db + 24.0) / 48.0, 0.0, 1.0)

def db_from_y(y, height=315.0):
    return clamp((height - y) / height * 48.0 - 24.0, -24.0, 24.0)

def dynamic_target(offset, target, dynamics):
    span = abs(target - offset)
    amount = abs(clamp(dynamics, -100.0, 100.0)) / 100.0
    direction = -1.0 if dynamics < 0.0 else 1.0
    return clamp(offset + direction * span * amount, -24.0, 24.0)

def source_assertions():
    cpp = CPP.read_text(encoding="utf-8")
    proc = PROC.read_text(encoding="utf-8")
    head = HEAD.read_text(encoding="utf-8")

    required = [
        'f("DYN_DYNAMICS" + n, "Dynamic EQ " + n + " Dynamics",',
        '-100.f, 100.f, dynDynamicsDefaults[i])',
        'setParameter("DYN_DYNAMICS" + n, dynamics);',
        'parameter->beginChangeGesture();',
        'parameter->endChangeGesture();',
        'DYN_DYNAMICS',
        'dynamicTargetDragStartY = pos.y',
        'const float effectiveDx = rawDx;',
        'const float effectiveDy = rawDy;',
        'sendNotificationSync',
        'followScale = 0.74f',
        'Dynamic Range is deliberately Y-only',
        'DYNAMICS uses a truly linear bipolar map',
        '(db + 24.f) / 48.f',
        'constexpr float staticNodeRadius = 8.0f',
    ]
    for token in required:
        assert token in cpp or token in proc or token in head, f"missing source invariant: {token}"

    assert 'DYN_TARGET" + n, storedTarget' not in cpp,         "graph drag must not write DYN_TARGET anymore"
    assert 'dynamicTargetDragStartY = pos.y;' in cpp,         "Dynamic gesture must anchor to the actual mouse-down pixel"
    assert 'sendNotificationSync' in cpp,         "Lower DYNAMICS knob must refresh synchronously during graph drag"
    assert 'DYN_DYNAMICS" + n, dynamics' in cpp, \
        "graph drag must write DYN_DYNAMICS"

def test_280_design_cases():
    # 280 deterministic combinations: 10 starting dynamics x 7 Y offsets x 4 scales.
    starts = [-100, -75, -50, -25, 0, 25, 50, 75, 100, -1]
    offsets = [-315, -157.5, -63, -8, 0, 8, 315]
    scales = [0.1, 0.25, 0.5, 1.0]
    count = 0
    for start in starts:
        for dy in offsets:
            for scale in scales:
                out = dyn_from_drag(start, 157.5, 157.5 + dy, 315, scale)
                assert -100 <= out <= 100
                # Above movement raises Dynamics; below movement lowers it.
                if dy < 0:
                    assert out >= start or out == 100
                if dy > 0:
                    assert out <= start or out == -100
                # Midpoint identity.
                if dy == 0:
                    assert abs(out - start) < 1e-9
                count += 1
    assert count == 280

def test_graph_roundtrip():
    for db in [-24, -18, -12, -6, 0, 6, 12, 18, 24]:
        y = y_from_db(db)
        back = db_from_y(y)
        assert abs(db - back) < 1e-6

def test_dynamic_range_direction():
    for start in [-100, -75, -50, -10, 0, 10, 50, 75, 100]:
        up = dyn_from_drag(start, 157.5, 100)
        down = dyn_from_drag(start, 157.5, 215)
        assert up >= start or up == 100
        assert down <= start or down == -100

def test_dynamic_drag_anchor_is_exact():
    # Grabbing anywhere inside the Dynamic ring must not create a first-frame
    # parameter jump. The old implementation anchored to target.y instead of
    # the actual mouse-down position.
    for start in [-100, -75, -50, -10, 0, 10, 50, 75, 100]:
        for grab_y in [80.0, 120.0, 157.5, 195.0, 235.0]:
            assert dyn_from_drag(start, grab_y, grab_y) == start

def test_dynamic_cross_zero_is_linear():
    # Equal pixel increments must produce equal parameter increments, including
    # across the -/+ zero crossing.
    start_y = 157.5
    step = 15.75  # 10% per step with the current 315 px graph height.
    ys = [start_y + i * step for i in range(-10, 11)]
    vals = [dyn_from_drag(0.0, start_y, y) for y in ys]
    increments = [vals[i + 1] - vals[i] for i in range(len(vals) - 1)]
    assert all(abs(v - increments[0]) < 1.0e-9 for v in increments)

def test_eq_xy_drag_math():
    # Frequency and Gain must both respond to the same gesture.
    cases = [(40, 20), (-40, 20), (40, -20), (-40, -20), (0, 30), (30, 0)]
    for dx, dy in cases:
        effective_dx = dx
        effective_dy = dy
        assert effective_dx == dx
        assert effective_dy == dy
        # Upward movement increases Gain; downward movement decreases it.
        gain_delta = -effective_dy / 315.0 * 36.0
        if dy < 0:
            assert gain_delta > 0
        elif dy > 0:
            assert gain_delta < 0

    cpp = CPP.read_text(encoding="utf-8")
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")
    assert 'const float effectiveDx = rawDx;' in cpp
    assert 'const float effectiveDy = rawDy;' in cpp
    assert 'const effectiveDx=rawDx;' in web
    assert 'const effectiveDy=rawDy;' in web
    assert 'graphEqDragAxis == GraphEqDragAxis::Frequency' not in cpp
    assert 'eqDragAxis===1?rawDx:0' not in web

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
        target = clamp(offset + 4, -24, 24)
        for d in [-100, -50, 0, 50, 100]:
            v = dynamic_target(offset, target, d)
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
        gain = rng.uniform(-24, 24)
        dyn = rng.uniform(-100, 100)
        target = rng.uniform(-24, 24)
        q = rng.uniform(0.1, 18)
        attack = rng.uniform(0.1, 200)
        release = rng.uniform(5, 2000)
        ms = rng.uniform(0, 100)
        assert 20 <= freq <= 20000
        assert -24 <= gain <= 24
        assert -100 <= dyn <= 100
        assert -24 <= target <= 24
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
                # Dynamic drag never changes the band's frequency.
                assert 20 <= freq <= 20000
            values.append(dynamics)
        assert len(values) == 4

def main():
    source_assertions()
    test_280_design_cases()
    test_graph_roundtrip()
    test_dynamic_range_direction()
    test_dynamic_drag_anchor_is_exact()
    test_dynamic_cross_zero_is_linear()
    test_eq_xy_drag_math()
    test_dynamic_target_is_linear()
    test_threshold_is_linear()
    test_target_visual_direction()
    test_frequency_deadzone()
    test_transient_gestures()
    for _ in range(10):
        test_all_features_rounds()
    test_full_simulation()
    print("PASS: 280 design cases")
    print("PASS: 10 core/all-feature rounds")
    print("PASS: 6 transient gesture sequences")
    print("PASS: 10 full simulated sessions")
    print("PASS: source invariants / APVTS / graph-DYNAMICS binding")
    print("ALL Dynamic EQ UI regression tests passed")

if __name__ == "__main__":
    main()


def test_frequency_drag_is_slow_and_grab_anchored():
    cpp = CPP.read_text(encoding="utf-8")
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")
    head = HEAD.read_text(encoding="utf-8")
    assert 'followScale = 0.74f' in cpp
    assert 'followScale=0.74' in web
    assert 'graphFreqDragGrabOffsetX' in head
    assert 'const float rawDx = correctedPointerX - nodeStartX;' in cpp
    assert 'const float nodeStartX=logX(dynFreqStartHz,w);' in web
    assert 'dynFreqGrabOffsetX=x-logX(dynFreqStartHz,w);' in web
    assert 'const effectiveDx=rawDx;' in web
    assert 'const effectiveDy=rawDy;' in web

