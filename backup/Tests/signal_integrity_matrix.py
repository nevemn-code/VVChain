#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
cpp = (ROOT / "Source" / "DSP" / "ChainDSP.cpp").read_text(encoding="utf-8")
hdr = (ROOT / "Source" / "DSP" / "ChainDSP.h").read_text(encoding="utf-8")
web = (ROOT / "docs" / "vvchain-worklet.js").read_text(encoding="utf-8")
rules = (ROOT / ".github" / "VVCHAIN_RULES.md").read_text(encoding="utf-8")

# UDMBC must be stereo-linked: one detector/gain state per band, not per channel.
assert "float gateEnvDb = 0.f;" in hdr
assert "std::array<float, 2> gateEnvDb" not in hdr
assert "const float linkedInput" in cpp
assert "std::sqrt(0.5f * (" in cpp
assert "UdmbcBandCoeffCache" in hdr
assert "udmbcBandCoeffCache[band]" in cpp
assert "cache.matches(" in cpp
assert "EqCoeffCache" in hdr
assert "dynMidEqCoeffCache[band].matches" in cpp
assert "dynMidDetectorCoeffCache[band].matches" in cpp
assert "udLinked=Array.from" in web
assert "Math.sqrt(.5*(dry[0][b]*dry[0][b]+dry[1][b]*dry[1][b]))" in web

# Shared band split must be LR4 in both Native and Web, not the old one-pole preview.
assert "updateCrossover(udmbcXover1" in cpp
assert "xoverPair(x,z,coef)" in web
assert "this.biquad(this.biquad(x,coef.lp,z.lp1),coef.lp,z.lp2)" in web
assert "lp[0]+=" not in web

# Fixed UDMBC time constants are computed outside the sample loop.
ud = cpp[cpp.index("void VVChainDSP::applyUdmbc"):cpp.index("void VVChainDSP::applyAType")]
hot_anchor = "const bool stereo = nCh > 1;"
assert hot_anchor in ud
hot = ud[ud.index(hot_anchor):]
sample_loop = hot[hot.index("for (int n = 0; n < buffer.getNumSamples(); ++n)"):]
assert "std::exp(" not in sample_loop
assert sample_loop.count("timeCoeff(") == 1  # only true programme-dependent lifter release remains dynamic
assert "timeCoeff(sr, upReleasePdr)" in sample_loop

# Type-A uses analytical ADAA on both Native and Web.
assert "processTypeAAdAA" in cpp
assert "typeAAdAA" in web
assert "std::tanh(bands[band] * driveParam[band])" not in cpp

# Large host blocks must be chunked, never silently bypassed.
assert "numSamples > preparedBlockCapacity" in cpp
assert "process(view, p);" in cpp
assert "juce::jmax(65536" in cpp

# Neutral limiter blocks keep oversampler state warm but return a pure delayed path.
assert "if (!anyGainReduction)" in cpp
assert "limiterDryDelay" in cpp + hdr

# Rules describe the current linked detector and LR4/Web parity.
assert "stereo-linked" in rules
assert "15 ms" in rules and "8 ms" in rules

print("PASS signal integrity matrix: stereo link, LR4 parity, lazy coefficients, Type-A ADAA, large-block and neutral-null guards")
