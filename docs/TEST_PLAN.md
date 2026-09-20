# VVChain Test Plan

## Deterministic reference regression

The requested verification matrix is implemented exactly:

| Stage | Cases | Purpose |
|---|---:|---|
| Planning / parameter-space | 280 | Randomized range sanitization and finite-value validation |
| Debug / boundary | 120 | Min/max, zero, ratio, timing and output-edge cases |
| All-feature sensitivity | 180 | Every exposed EQ / OTT / A-Type / De-Esser / Mix control is changed and checked for observable output effect |
| Transient | 655 | Impulse, double-hit, burst, alternating and mixed transient stress |
| Full-chain | 820 | Full signal path across 44.1/48/88.2/96/192 kHz and 16–1024 sample blocks, including loop-range math |
| **Total** | **2,055** | |

Additional web smoke checks verify the required drag/drop, waveform selection, loopStart/loopEnd, module controls and animation surface are present in \`docs/index.html\`.

## Host validation

- JUCE/CMake build
- pluginval
- REAPER
- Cubase
- Pro Tools for AAX
- mono and stereo
- 44.1 / 48 / 88.2 / 96 / 192 kHz
- small and large blocks
- automation
- state save/restore
- bypass and wet/dry checks
- denormal / NaN / infinity checks
- preset recall
