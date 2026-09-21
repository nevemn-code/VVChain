#!/usr/bin/env python3
from __future__ import annotations

import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HTML = ROOT / "docs" / "index.html"
text = HTML.read_text(encoding="utf-8")

match = re.search(r"<script>([\s\S]*)</script>", text, re.I)
assert match, "missing main script"
script = match.group(1)

node = subprocess.run(
    ["node", "-e", "const fs=require('fs'); const s=fs.readFileSync(0,'utf8'); new Function(s);"],
    input=script, text=True, encoding="utf-8", capture_output=True,
)
assert node.returncode == 0, node.stderr

wm = re.search(r'''const WORKLET_SOURCE=(["'](?:\\.|[^"'])*["'])s*;''', script, re.S)
assert wm, "missing WORKLET_SOURCE"
worklet = json.loads(wm.group(1))

node = subprocess.run(
    ["node", "-e", "const fs=require('fs'); const s=fs.readFileSync(0,'utf8'); new Function(s);"],
    input=worklet, text=True, encoding="utf-8", capture_output=True,
)
assert node.returncode == 0, node.stderr

required = [
    "LOAD AUDIO", "loopStart", "loopEnd", "loopToggle",
    "AudioWorkletNode", "audioWorklet.addModule", "vvchain-worklet",
    "8192", "12500", "13500", "DE-ESSER", "Male Vocal", "Female Vocal",
    "INTENSITY", "OFFSET", "BYPASS", "PROCESS DE-ESSER",
    "ALL MODULES ON ONE PAGE", "OTT / PUNKOTT-MB", "TYPE-A", "MIX / OUT",
    "eqCards", "ottBands", "typeBands", "deKnob0", "mixGrid",
]
for token in required:
    assert token in text, token

for forbidden in [
    "ANALYZER", "analyzer", "createAnalyser", "AnalyserNode", "functionTabs",
    "inputAnalyser", "outputAnalyser", "fftSize",
    "4096", "1365", "2730", "2731",
    "N=4096", "HOP=1365", "SHIFT=2730",
    "let workletNode = null,  = null",
]:
    assert forbidden not in text, f"forbidden/legacy remains: {forbidden}"

print("VVChain web smoke test: PASS")
print("main_js_parse: PASS")
print("worklet_js_parse: PASS")
print("analyzer_code: REMOVED")
print("continuous_deesser_stream: 8192-block")
