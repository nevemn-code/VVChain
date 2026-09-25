#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

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

    # Main Spectrum must remain present while module contribution analyzers
    # and their runtime buffers/FFT/Worklet messages are fully absent.
    assert "analyzerPath.cubicTo" in editor
    assert "ContributionFFT" not in web
    assert "contributionCurves" not in web
    assert 'type:"analysisEnabled"' not in worklet
    assert 'type:"contributionSamples"' not in worklet

    # CPU architecture invariants for v1.0.60.
    assert "Nonlinear Analog ADAA v2 remains fixed at 4x" in dsp
    assert "eqOversampler.processSamplesUp" in dsp
    assert "const double osSr = sr;" in dsp
    assert "dynamicsAmount <= 0.000001f" in dsp
    assert dsp.count("bool anyActiveBand = false;") >= 2
    assert "eqWetDelay" in dsp

    print("PASS analyzer matrix: main Spectrum only + module analyzer removal + CPU lazy-path invariants")

if __name__ == "__main__":
    main()
