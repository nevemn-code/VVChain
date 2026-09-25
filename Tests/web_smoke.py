#!/usr/bin/env python3
from __future__ import annotations
import re
import subprocess
from html.parser import HTMLParser
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HTML = ROOT / "docs" / "index.html"
WORKLET = ROOT / "docs" / "vvchain-worklet.js"
SOURCE = ROOT / "Source" / "PluginEditor.cpp"
CMAKE = ROOT / "CMakeLists.txt"

text = HTML.read_text(encoding="utf-8")

# A tooltip can have correct text and CSS but still be clipped below the canvas
# if its element never receives the positioning class.
class HintParser(HTMLParser):
    hints = []
    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if attrs.get('id') == 'graphHint':
            self.hints.append(attrs)

hint_parser = HintParser()
hint_parser.feed(text)
assert len(hint_parser.hints) == 1
assert 'graphHint' in hint_parser.hints[0].get('class', '').split(), 'floating hint is missing its positioning class'

worklet = WORKLET.read_text(encoding="utf-8")
source = SOURCE.read_text(encoding="utf-8")
cmake = CMAKE.read_text(encoding="utf-8")

match = re.search(r"<script(?:\s[^>]*)?>([\s\S]*)</script>", text, re.I)
assert match, "missing main inline script"
script = match.group(1)

node = subprocess.run(
    ["node", "-e", "new Function(require('fs').readFileSync(0,'utf8'));"],
    input=script, text=True, encoding="utf-8", capture_output=True,
)
assert node.returncode == 0, node.stderr

node = subprocess.run(
    ["node", "--check", str(WORKLET)],
    text=True, encoding="utf-8", capture_output=True,
)
assert node.returncode == 0, node.stderr

# Current Web architecture smoke gates.
required_html = [
    "LOAD AUDIO",
    "AudioWorkletNode",
    "new URL(\"vvchain-worklet.js\",document.baseURI)",
    'id="bandGrid"',
    "grid-template-columns:repeat(4,minmax(0,1fr)) 1fr",
    "UDMBC %",
    "ANALOG COLOR",
    "TAPE COLOR +",
    "TRANSIENT",
    "masterBypassLabel",
    "graphHintBandHtml",
    "resetGraphGainAtDoubleClick",
    'eqCanvas.addEventListener("dblclick",resetGraphGainAtDoubleClick)',
    "knobRefreshers.forEach(fn=>fn())",
    "state.eq.gain[staticBand]=0",
    "state.dyn.dynamics[dynamicBand]=0",
    "dynDetectBlend",
    "moduleMuteRefreshers",
    'id="uiThemeQuick"',
    "UI STUDIO",
    "UI IVORY",
    "vvchain-v1064-dual-hardware-skins",
    "vvchain-v1067-real-png-assets",
    "assets/ui/png/studio_knob_strip.png",
    "assets/ui/png/ivory_knob_strip.png",
    "assets/ui/png/studio_button_on.png",
    "assets/ui/png/ivory_led_on.png",
]
for token in required_html:
    assert token in text, token

required_worklet = [
    "registerProcessor",
    "tptBell",
    "analog(x,alpha,ch,b,x2=1)",
    "analytical first-order ADAA",
    "analogReconstructed",
    'zoneBands(y,c,"analogLp",s.udmbc.x)',
    "colorX2?.[b]?2:1",
]
for token in required_worklet:
    assert token in worklet, token

for token in [
    "applyTransientStereo(l,r,stereo,xs)",
    "transientFast",
    "transientSlow",
    "transientGain",
    "deltaL+=bandsL[b]*d",
    "const transientOut=this.applyTransientStereo",
]:
    assert token in worklet, token

for forbidden in ["DEESS_", "deessStereo", "state.de.", "de:{", "DE-ESSER"]:
    assert forbidden not in text + worklet + source, forbidden

cmake_version = re.search(r"project\(VVChain VERSION (\d+\.\d+\.\d+)", cmake)
shown_version = re.search(r"VVCHAIN v(\d+\.\d+\.\d+)", text)
cache_version = re.search(r'vvchain-worklet\.js",document\.baseURI\)\.href\+"\?v=(\d+\.\d+\.\d+)"', text)
assert cmake_version and shown_version and cache_version
assert cmake_version.group(1) == shown_version.group(1) == cache_version.group(1), (
    cmake_version.group(1), shown_version.group(1), cache_version.group(1)
)
assert not (ROOT / "docs" / "legacy analog-copper.html").exists(), "obsolete legacy analog page must remain deleted"

# DELTA analyzer: original/input reference is disconnected and the final
# Worklet output becomes the only main analyzer source.
for token in [
    "function refreshAnalyzerTap()",
    "state.delta&&!directFallback&&!workletFaulted&&workletNode",
    "workletNode.connect(analyserNode)",
]:
    assert token in text, token
# Main Spectrum remains; module contribution analyzer is removed.
for token in [
    "analyserNode.fftSize=4096",
    "analyserNode.smoothingTimeConstant=0",
]:
    assert token in text, token
assert 'type:"contributionSamples"' not in worklet

assert "void VVChainAudioProcessorEditor::mouseDoubleClick" in source
assert 'resetParameter("EQ" + n + "_GAIN", 0.0f)' in source
assert 'resetParameter("DYN_DYNAMICS" + n, 0.0f)' in source
assert "Horizontal = Frequency" in source or "Horizontal = the same linked EQ Frequency parameter." in source
assert "const dbTicks=[18,15,12,9,6,3,0,-3,-6,-9,-12,-15,-18]" in text
assert '[20,"20"],[30,"30"],[40,"40"],[50,"50"],[70,"70"],[100,"100"]' in text
assert "if(db===15||db===-15)return;" in text
assert ".graphHint{width:150px;min-width:150px;max-width:150px}" in text
assert "font:700 10px/15px Consolas" in text
assert "constexpr int boxWidth = 150" in (ROOT / "Source" / "PluginEditor.h").read_text(encoding="utf-8")
assert "FontOptions(10.5f)" in (ROOT / "Source" / "PluginEditor.h").read_text(encoding="utf-8")

for forbidden in [
    "analog-v1.html",
    "analog-v2.html",
    "analog-v3.html",
]:
    assert forbidden not in text, f"legacy/forbidden remains: {forbidden}"

print(f"PASS Web smoke: v{cmake_version.group(1)} parity + smooth main analyzer only")
