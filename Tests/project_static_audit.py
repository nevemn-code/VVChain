#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

cmake = read("CMakeLists.txt")
editor = read("Source/PluginEditor.cpp")
editor_h = read("Source/PluginEditor.h")
dsp = read("Source/DSP/ChainDSP.cpp")
dsp_h = read("Source/DSP/ChainDSP.h")
web = read("docs/index.html")
build = read(".github/workflows/build.yml")
pages = read(".github/workflows/pages.yml")
rules = read(".github/VVCHAIN_RULES.md")
root_rules = read("PROJECT_RULES.md")
stress = read("Tests/reference_stress.py")

version = re.search(r"project\(VVChain VERSION (\d+\.\d+\.\d+)", cmake)
shown = re.search(r"VVCHAIN v(\d+\.\d+\.\d+)", web)
native = re.search(r"VVCHAIN v(\d+\.\d+\.\d+)", editor)
cache = re.search(r'vvchain-worklet\.js",document\.baseURI\)\.href\+"\?v=(\d+\.\d+\.\d+)"', web)
artifact = re.search(r"VVChain-v(\d+\.\d+\.\d+)-Windows-VST3", build)
assert version and shown and native and cache and artifact
assert len({version.group(1), shown.group(1), native.group(1), cache.group(1), artifact.group(1)}) == 1

for dead in ("graphHintBand", "graphHintAutoHideAt", "graphHintActiveMask"):
    assert dead not in editor + editor_h, dead

for dead in ("analogTempBuffer", "analogBandBuffers", "analogSourceBuffer"):
    assert dead not in dsp + dsp_h, dead

assert not (ROOT / "docs" / "legacy analog-copper.html").exists()
assert "×1.6" not in rules + root_rules
assert "x1.6" not in (rules + root_rules).lower()
assert "delta ×2" in root_rules
assert "analytical first-order ADAA" in rules
assert "ADAA state" in rules
assert "analogXover1.low" in dsp
assert "analogXover2.low" in dsp
assert "analogXover3.low" in dsp

assert "github.run_id" not in build
assert "vvchain-win-vst3-juce-9.0.2-vs2026-v1" in build
assert "--target VVChain_VST3" in build
assert "VVCHAIN_COPY_PLUGIN_AFTER_BUILD=OFF" in build
assert "github.event_name == 'pull_request' || github.event_name == 'push' || github.event_name == 'workflow_dispatch'" in build
assert "scipy" not in build.lower()

assert "enable_testing()" not in cmake
assert "vvchain_reference_stress" not in cmake
assert "VVCHAIN_COPY_PLUGIN_AFTER_BUILD" in cmake

assert "timeout-minutes: 3" in pages
assert "Web visible version and AudioWorklet cache version differ" in pages

assert "tone - tone" not in stress
assert "scipy" not in stress.lower()
assert ".github/VVCHAIN_RULES.md" in root_rules
assert "if (dragXover < 0 && dragOverlapXover < 0)" in editor

processor_h = read("Source/PluginProcessor.h")
processor = read("Source/PluginProcessor.cpp")
settings = read("Source/SettingsPanel.cpp")

# v1.0.60 analyzer/transient invariants: main Spectrum only, no module contribution path.
assert "analyzerFftOrder = 12" in editor_h
assert "analyzerHopSize = analyzerFftSize / 2" in editor_h
assert "contributionFftOrder" not in editor_h
assert "std::atomic<bool> analyzerEnabled { false }" in processor_h
assert "popContributionSamples" not in processor_h + processor
assert "setContributionAnalysisEnabled" not in dsp_h + processor
assert "contributionStream" not in dsp_h + processor
assert "analyzerPath.cubicTo" in editor
assert "Nonlinear Analog ADAA v2 remains fixed at 4x" in dsp
assert "eqOversampler.processSamplesUp" in dsp
assert "const double osSr = sr;" in dsp
assert "dynamicsAmount <= 0.000001f" in dsp
assert dsp.count("bool anyActiveBand = false;") >= 2
assert "eqWetDelay" in dsp + dsp_h
assert "visibilitychange" in web
assert "const bool deltaMonitorOn" in processor
assert "if (!deltaMonitorOn)" in processor
assert "if (analyzerOn && p.deltaMonitor)" in processor
assert 'parameterValue("DELTA_MONITOR") > 0.5f' in editor
assert "refreshAnalyzerTap" in web

# TRANSIENT is base-rate, precedes Analog, uses a stereo-linked detector,
# and injects only band deltas so it cannot add a second full-band crossover
# phase rotation. It must not wake the Analog 4x path by itself.
assert "transientAmount" in dsp_h + processor
assert "void VVChainDSP::applyTransient" in dsp
assert "applyTransient(buffer, p);" in dsp
assert "vvFastLogPositive" in dsp
assert "transientBand1SidechainHPF" in dsp_h + dsp
assert "transientXover1" in dsp_h + dsp
assert "const float energySq" in dsp
assert "inputByChannel[ch] + delta" in dsp
assert "bandsByChannel[ch][band] * (gains[band] - 1.0f)" in dsp
assert "anyBandStageActive" not in dsp
assert "transientGain[band]" not in dsp
for dead in ("DEESS_", "processDeEsser", "DeEssState", "deessStereo", "state.de.", "de:{"):
    assert dead not in editor_h + editor + processor_h + processor + dsp_h + dsp + web + read("docs/vvchain-worklet.js"), dead

print(f"PASS project static audit v{version.group(1)}")
