#!/usr/bin/env python3
from __future__ import annotations
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HTML = ROOT / "docs" / "index.html"
WORKLET = ROOT / "docs" / "vvchain-worklet.js"

text = HTML.read_text(encoding="utf-8")
worklet = WORKLET.read_text(encoding="utf-8")

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

# Current v1.0.23 Web architecture smoke gates only.
required_html = [
    "LOAD AUDIO",
    "AudioWorkletNode",
    "new URL(\"vvchain-worklet.js\",document.baseURI)",
    'id="bandGrid"',
    "grid-template-columns:repeat(4,minmax(0,1fr)) .5fr .5fr",
    "OTT %",
    "ANALOG COLOR",
    "TAPE-A +",
    "DE-ESSER",
    "MAXIMUM REDUCTION",
    "masterBypassLabel",
    "graphHintBandHtml",
    "dynDetectBlend",
    "moduleMuteRefreshers",
    "globalCompositeOperation=\"saturation\"",
]
for token in required_html:
    assert token in text, token

required_worklet = [
    "registerProcessor",
    "tptBell",
    "analog(x,a,ss,ch,b,x2=1)",
    "colorX2?.[b]?2:1",
]
for token in required_worklet:
    assert token in worklet, token

for forbidden in [
    "analog-v1.html",
    "analog-v2.html",
    "analog-v3.html",
]:
    assert forbidden not in text, f"legacy/forbidden remains: {forbidden}"

print("PASS Web smoke: current external Worklet + v1.0.23 UI structure")
