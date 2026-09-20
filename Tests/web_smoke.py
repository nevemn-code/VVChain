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
    input=script,
    text=True,
    encoding="utf-8",
    capture_output=True,
)
assert node.returncode == 0, node.stderr

wm = re.search(r'''const WORKLET_SOURCE\s*=\s*("(?:\\.|[^"])*")\s*;''', script, re.S)
assert wm, "missing WORKLET_SOURCE"
worklet = json.loads(wm.group(1))

node = subprocess.run(
    ["node", "-e", "const fs=require('fs'); const s=fs.readFileSync(0,'utf8'); new Function(s);"],
    input=worklet,
    text=True,
    encoding="utf-8",
    capture_output=True,
)
assert node.returncode == 0, node.stderr

required = [
    "LOAD AUDIO",
    "loopStart",
    "loopEnd",
    "loopToggle",
    "AudioWorkletNode",
    "audioWorklet.addModule",
    "vvchain-worklet",
    "8192",
    "DE-ESSER",
    "TYPE-A",
    "OTT",
    "EQ / ANALOG",
    "FFT",
    "dragover",
    "drop",
]
for token in required:
    assert token in text, token

for forbidden in [
    '$("#rangeStart")',
    '$("#rangeEnd")',
    '$("rangeStart")',
    '$("rangeEnd")',
    "async function processBuffer",
    "await processBuffer(audioBuffer)",
    "AudioBuffer({length",
]:
    assert forbidden not in text, f"forbidden/legacy pattern remains: {forbidden}"

print("VVChain web smoke test: PASS")
print("main_js_parse: PASS")
print("worklet_js_parse: PASS")
print("blocking_whole_file_processor: REMOVED")
