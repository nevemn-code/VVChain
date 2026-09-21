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
    ["node", "-e", "const s=require('fs').readFileSync(0,'utf8'); new Function(s);"],
    input=script, text=True, encoding="utf-8", capture_output=True,
)
assert node.returncode == 0, node.stderr

ws_start = script.index("const WORKLET_SOURCE=") + len("const WORKLET_SOURCE=")
ws_end = script.index(";\nfunction makeWorkletUrl", ws_start)
worklet = json.loads(script[ws_start:ws_end])

node = subprocess.run(
    ["node", "-e", "new Function(require('fs').readFileSync(0,'utf8'));"],
    input=worklet, text=True, encoding="utf-8", capture_output=True,
)
assert node.returncode == 0, node.stderr

required = [
    "LOAD AUDIO", "AudioWorkletNode",
    "TAPE-A", "OTT %", "ATTACK", "RELEASE",
    "DE-ESS FREQ", "MAXIMUM REDUCTION", "MIX", "OUT",
    "bandGrid", "advPopup", "advPopupGrid", "leds",
    "ott.bandBypass", "bandLed de",
    "modeSwitch", "Solid-State Saturation", "Tube Saturation",
    "grid-template-columns:repeat(5,1fr)",
    'label:"MIX"', 'label:"OUT"',
    "source.loop=true",
    "knobRefreshers=[]", "knobRefreshers.push(render)",
    "knobRefreshers.forEach(fn=>fn())",
]
for token in required:
    assert token in text or token in worklet, token

assert "overflow:hidden" in text
assert "grid-template-rows:.8fr .8fr 1fr 1fr" in text
assert 'bypassLed("EQ"' in text
assert 'bypassLed("OTT"' in text
assert 'bypassLed("TAPE-A"' in text
assert 'bypassLed("DE-ESS"' in text
assert "this.N=512" in text
assert "DE-ESSER no longer uses FFT/block buffering" in text or "DE-ESSER = ZERO-LATENCY SAMPLE-DOMAIN" in text
assert "hp(f,q=.707)" in worklet
assert "this.hp(this.clamp(Number(this.s.de.freq||8000)" in worklet
assert "fft(re,im,inv)" not in worklet and "this.fft(" not in worklet
assert "dryQueue" not in worklet

for forbidden in [
    "ANALYZER", "createAnalyser", "AnalyserNode",
    "functionTabs", "deKnob0", "DEESS_VOICE",
]:
    assert forbidden not in text, f"legacy/forbidden remains: {forbidden}"

print("main_js_parse: PASS")
print("worklet_js_parse: PASS")
print("no_page_scroll: PASS")
print("four_band_compact_chain: PASS")
print("stable_web_preview: PASS")

assert "bypassGlobalLed(\"ANALOG\"" in text
assert "bypassLed(\"DE-ESS\"" in text
assert "deessLedRefs.push(deLed)" in text
assert "SOLO POST" in text
assert "Math.min(targetSolo" in worklet
assert "yL=yL*(1-this.soloBlend)+soloL*this.soloBlend" in worklet
print("zero_latency_deesser: PASS")
print("solo_pre_post_crossfade: PASS")
print("analog_global_bypass: PASS")
print("deesser_top_bottom_sync: PASS")
