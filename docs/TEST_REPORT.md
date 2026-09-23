# VVChain Reference Test Report

## v1.0.9 TPT Bell verification

The candidate TPT Bell core passed 50 deterministic transfer-function cases before repository replacement.

- Maximum magnitude-response difference versus independent RBJ reference: 2.54e-8 dB.
- Maximum phase-response difference: 6.71e-9 rad.
- Maximum centre-frequency gain error: 3.41e-11 dB.
- 0 dB identity error: exactly 0 in the double-precision reference run.
- A separate 50-case per-sample four-band modulation stress run produced no NaN/Inf; worst output and filter states remained bounded.
The current deterministic matrix contains **2,485 cases**:

- Planning: 280
- Debug continuity: 500
- All-feature: 180
- Transient: 155
- Full-chain: 220
- Independent band bypass: 50
- Analog color unity: 50
- TT / SS saturation: 500
- Type-A exciter: 50
- OTT four-band: 500

This report is updated by CI runs. The latest run before the current patch failed in the new analog unity micro-signal check; the DSP was then updated with an exact unity guard below the nonlinear noise floor.

Real VST3 host/pluginval and AAX SDK/signing validation remains a separate release requirement.
