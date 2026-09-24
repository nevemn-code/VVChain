# VVChain Validation Status

This document records what the repository actually verifies. It is not a claim of DAW certification.

## Automated PR gate

The current fast gate checks:

- Native/Web source synchronization.
- Web and AudioWorklet JavaScript syntax.
- Web smoke invariants.
- Dynamic EQ / graph / bypass / interaction regressions.
- Compact EQ / DYN EQ floating readout behavior.
- Shared continuous Q-wheel behavior.

## Manual full validation

The manual path builds Native VST3 and runs the expensive Analog and DSP reference tests.

## Known validation boundary

Repository source tests do not replace real host validation. Windows VST3 compilation, pluginval, target DAWs, automation/state recall, mono/stereo, sample-rate changes and AAX signing remain separate release checks.

## Historical TPT Bell reference

The v1.0.9 independent reference run recorded very small transfer-function differences against its comparison implementation and no NaN/Inf in its modulation stress cases. Those figures are historical reference data, not a claim that every later release automatically reran the same external comparison.
