# VVChain Reference Test Report

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
