#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    editor = (ROOT / "Source" / "PluginEditor.cpp").read_text(encoding="utf-8")
    editor_h = (ROOT / "Source" / "PluginEditor.h").read_text(encoding="utf-8")
    dsp = (ROOT / "Source" / "DSP" / "ChainDSP.cpp").read_text(encoding="utf-8")
    dsp_h = (ROOT / "Source" / "DSP" / "ChainDSP.h").read_text(encoding="utf-8")
    processor = (ROOT / "Source" / "PluginProcessor.cpp").read_text(encoding="utf-8")
    processor_h = (ROOT / "Source" / "PluginProcessor.h").read_text(encoding="utf-8")
    web = (ROOT / "docs" / "index.html").read_text(encoding="utf-8")
    worklet = (ROOT / "docs" / "vvchain-worklet.js").read_text(encoding="utf-8")

    # Main analyzer remains 4096 FFT / 50% overlap and is editor gated.
    assert "analyzerFftOrder = 12" in editor_h
    assert "analyzerHopSize = analyzerFftSize / 2" in editor_h
    assert "audioProcessor.setAnalyzerEnabled(analyzerEnabled && isShowing())" in editor
    assert "audioProcessor.setAnalyzerEnabled(false)" in editor
    assert "std::atomic<bool> analyzerEnabled { false }" in processor_h
    assert "if (!analyzerOn)" in processor

    # DELTA uses only the final audible output; normal mode uses pre-DSP input.
    assert "const bool deltaMonitorOn" in processor
    assert "if (!deltaMonitorOn)" in processor
    assert "if (analyzerOn && p.deltaMonitor)" in processor
    assert "function refreshAnalyzerTap()" in web
    assert "state.delta&&!directFallback&&!workletFaulted&&workletNode" in web
    assert "workletNode.connect(analyserNode)" in web
    assert "source.connect(analyserNode)" in web

    # Hidden Web UI must disconnect the AnalyserNode itself, not merely stop paint.
    assert "disconnectAnalyzerTap()" in web
    assert 'document.addEventListener("visibilitychange"' in web
    assert "document.hidden){stopAnalyzerLoop();disconnectAnalyzerTap();" in web

    # Module contribution analyzers were intentionally removed for zero CPU.
    forbidden = (
        "contributionFftOrder",
        "ContributionFFT",
        "processContributionSamples",
        'type:"contributionSamples"',
        "popContributionSamples",
        "setContributionAnalysisEnabled",
        "contributionStream",
        "captureContributionMono",
        "contributionEqDownsampler",
    )
    combined = "\n".join((editor, editor_h, dsp, dsp_h, processor, processor_h, web, worklet))
    for token in forbidden:
        assert token not in combined, token

    # Main spectrum still exists in both Native and Web.
    assert "analyzerPath.cubicTo" in editor
    assert "analyserNode.fftSize=4096" in web
    assert "analyserNode.smoothingTimeConstant=0" in web

    print("PASS analyzer matrix: main 4096 FFT + DELTA routing + zero module-contribution compute")


if __name__ == "__main__":
    main()
