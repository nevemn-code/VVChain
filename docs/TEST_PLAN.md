# VVChain Test Plan

## Deterministic regression matrix

| Stage | Cases | Validation |
|---|---:|---|
| Planning / parameter-space | 280 | Parameter ranges, crossover ordering, finite state values |
| Boundary / debug | 120 | Min/max settings, zero settings, ratios, timing, output |
| All-feature mapping | 180 | Each exposed EQ / OTT / Type-A / De-Esser / Mix control changes the state signature and remains finite |
| Transient | 655 | Impulse, double hit, alternating burst, decaying burst, mixed transients |
| Full-chain | 820 | Full chain across 44.1 / 48 / 88.2 / 96 / 192 kHz and 16–1024 sample blocks, including loop-range geometry |
| **Total** | **2,055** | |

## Architecture-specific checks

- Four OTT bands exist and have independent degree controls.
- OTT has three crossover frequencies.
- OTT implements downward compression followed by upward compression.
- Type-A has four fixed Dolby-A-style bands with overlapping upper bands.
- Type-A band degree can independently be reduced to zero.
- De-esser uses two crossover edges.
- De-esser range and strength are independent.
- EQ has a nonlinear analog-style coloration stage that remains active at default color.
- Web preview contains AudioWorklet DSP, range selection, loopStart, and loopEnd.

## Host validation

Still required before release:

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
- AAX SDK build and signing
