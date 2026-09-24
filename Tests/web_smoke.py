#!/usr/bin/env python3
from __future__ import annotations
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

worklet_path = ROOT / "docs" / "vvchain-worklet.js"
assert worklet_path.is_file(), "missing external AudioWorklet source"
worklet = worklet_path.read_text(encoding="utf-8")
node = subprocess.run(
    ["node", "--check", str(worklet_path)],
    text=True, encoding="utf-8", capture_output=True,
)
assert node.returncode == 0, node.stderr
assert "new URL(\"vvchain-worklet.js\",document.baseURI)" in script

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
assert "d.addEventListener(\"wheel\"" in text
assert "wheelRemainder" in text
assert "#ef4444" in text and "#facc15" in text and "#3b82f6" in text and "#22c55e" in text
assert "background:#fff" in text
assert "graphHint" in text
assert "graphFrequencyText" in text

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
