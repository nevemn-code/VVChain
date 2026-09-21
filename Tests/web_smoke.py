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

ws_start = script.index("const WORKLET_SOURCE=") + len("const WORKLET_SOURCE=")
ws_end = script.index(";\nfunction makeWorkletUrl", ws_start)
worklet_literal = script[ws_start:ws_end]
worklet = json.loads(worklet_literal)
node = subprocess.run(
    ["node", "-e", "new Function(require('fs').readFileSync(0,'utf8'));"],
    input=worklet, text=True, encoding="utf-8", capture_output=True,
)
assert node.returncode == 0, node.stderr

required = [
    "LOAD AUDIO", "AudioWorkletNode", "8192",
    "TAPE-A", "OTT %", "ATTACK", "RELEASE",
    "DE-ESS FREQ", "DE-ESS INT",
    "bandGrid", "advBody", "mixBody", "leds",
    "NO PAGE SCROLL", "s.freq", "const avg=s.type.degree.reduce",
]
for token in required:
    assert token in text or token in worklet, token

assert "overflow:hidden" in text
assert "BYPASS" in text
assert "eq:{bypass:false" in text
assert "ott:{bypass:false" in text
assert "type:{bypass:false" in text
assert "de:{bypass:false" in text
assert "mix:{bypass:false" in text
assert "bypassLed(\"EQ\"" in text and "bypassLed(\"OTT\"" in text
assert "bypassLed(\"TAPE-A\"" in text and "bypassLed(\"DE-ESS\"" in text
assert "bypassLed(\"MIX\"" in text

for forbidden in [
    "ANALYZER", "analyzer", "createAnalyser", "AnalyserNode",
    "functionTabs", "ottBands", "typeBands", "deKnob0",
    "4096", "1365", "2730", "2731", "DEESS_VOICE",
]:
    assert forbidden not in text, f"legacy/forbidden remains: {forbidden}"

print("VVChain compact UI smoke test: PASS")
print("main_js_parse: PASS")
print("worklet_js_parse: PASS")
print("no_page_scroll: PASS")
print("four_band_compact_chain: PASS")
print("tape_a_active_amount: PASS")
print("band4_selectable_deesser_frequency: PASS")
print("led_bypass_controls: PASS")
print("continuous_deesser_stream: 8192-block")
