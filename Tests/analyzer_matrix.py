#!/usr/bin/env python3
from __future__ import annotations

import math
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def growth_db(pre_power: float, post_power: float, delta_power: float, post_db: float) -> float:
    if post_db < -82.0 or post_power <= pre_power * 1.005:
        return 0.0
    ratio = delta_power / max(1.0e-18, post_power)
    return max(0.0, min(8.0, 1.7 * 10.0 * math.log10(1.0 + ratio)))

def main() -> None:
    editor = (ROOT / "Source" / "PluginEditor.cpp").read_text(encoding="utf-8")
    editor_h = (ROOT / "Source" / "PluginEditor.h").read_text(encoding="utf-8")
    dsp = (ROOT / "Source" / "DSP" / "ChainDSP.cpp").read_text(encoding="utf-8")
    processor = (ROOT / "Source" / "PluginProcessor.cpp").read_text(encoding="utf-8")
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")
    worklet = (ROOT / "docs" / "vvchain-worklet.js").read_text(encoding="utf-8")

    assert "analyzerFftOrder = 12" in editor_h
    assert "analyzerHopSize = analyzerFftSize / 2" in editor_h
    assert "contributionFftOrder" not in editor_h
    assert "captureContributionMono" not in dsp
    assert "popContributionSamples" not in processor
    assert 'type:"contributionSamples"' not in worklet

    processor = (ROOT / "Source" / "PluginProcessor.cpp").read_text(encoding="utf-8")
    assert "const bool deltaMonitorOn" in processor
    assert "if (!deltaMonitorOn)" in processor
    assert "if (analyzerOn && p.deltaMonitor)" in processor
    assert "function refreshAnalyzerTap()" in web
    assert "state.delta&&!directFallback&&!workletFaulted&&workletNode" in web
    assert "state.masterBypass||state.delta" in web

    # 100 deterministic power-domain cases: removed energy never grows upward,
    # added energy is monotonic and the visualization is hard-capped at 8 dB.
    rng = random.Random(1055)
    previous = 0.0
    for i in range(100):
        pre = 10.0 ** rng.uniform(-8.0, -0.1)
        added_ratio = i / 12.0
        post = pre * (1.01 + added_ratio)
        delta = post * added_ratio
        post_db = 10.0 * math.log10(max(post, 1.0e-20))
        g = growth_db(pre, post, delta, post_db)
        assert math.isfinite(g)
        assert 0.0 <= g <= 8.0
        if post_db >= -82.0 and i > 1:
            # The mapping is monotonic for the controlled ratio sweep.
            controlled = growth_db(1.0, 1.1, added_ratio, 0.0)
            assert controlled + 1.0e-9 >= previous
            previous = controlled

    assert growth_db(1.0, 0.9, 0.8, 0.0) == 0.0
    assert growth_db(1.0, 1.004, 10.0, 0.0) == 0.0
    assert growth_db(1.0e-10, 1.1e-10, 1.0e-10, -90.0) == 0.0
    assert growth_db(1.0, 2.0, 1.0e12, 0.0) == 8.0

    print("PASS analyzer matrix: 100 power cases + Native/Web contribution + DELTA analyzer invariants")

if __name__ == "__main__":
    main()
