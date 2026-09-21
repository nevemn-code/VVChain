# VVChain Test Plan

## Deterministic regression matrix

| Stage | Cases | Validation |
|---|---:|---|
| Planning / parameter-space | 280 | Parameter ranges and crossover ordering |
| Boundary / debug continuity | 500 | 8192-sample stream continuity and finite values |
| All-feature mapping | 180 | EQ / OTT / TAPE-A / De-Esser / Mix controls remain finite |
| Transient | 155 | Impulse and transient handling |
| Full-chain | 220 | 44.1 / 48 / 88.2 / 96 / 192 kHz and 16–1024 sample blocks |
| Independent band bypass | 50 | OTT and TAPE-A per-band bypass independence |
| Analog color unity | 50 | Small-signal unity across 0–100% color |
| TT / SS saturation | 500 | 500 deterministic mode/amount sweeps |
| Type-A exciter | 50 | Four-band dynamic harmonic model |
| OTT four-band | 500 | Ratio, detector and gate behavior |
| **Total** | **2,485** | |

## Architecture-specific checks

- Four OTT bands with independent degree and bypass.
- Three Shared X-Over frequencies.
- TAPE-A follows the same Shared X-Over.
- Each EQ band has its own Analog Color amount.
- Each EQ band has TT / SS mode selection.
- De-Esser Maximum Reduction is 0–8 dB with 0 dB default.
- Visual FFT analyzer is intentionally absent from the current Native editor.
- De-Esser may use internal FFT processing when the effect is enabled.
- Master BYPASS must reach DSP and output the fixed-PDC delayed dry path.

## Host validation

Still required before release:

- JUCE/CMake build
- pluginval
- REAPER / Cubase / Pro Tools
- mono and stereo
- 44.1 / 48 / 88.2 / 96 / 192 kHz
- small and large blocks
- automation
- state save/restore
- bypass and wet/dry checks
- denormal / NaN / infinity checks
- preset recall
- AAX SDK build and signing
