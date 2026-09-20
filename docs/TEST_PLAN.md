# VVChain Test Plan

## Reference regression matrix

- 1280 planning/parameter cases
- 120 boundary/debug cases
- 180 all-feature interaction cases
- 655 transient cases
- 820 full-chain block simulations

Total: 3055 deterministic reference cases.

## Host validation

- JUCE/CMake build
- pluginval
- REAPER
- Cubase
- Pro Tools for AAX
- mono and stereo
- 44.1/48/88.2/96/192 kHz
- small and large blocks
- automation
- state save/restore
- bypass and wet/dry null checks
- denormal/NaN/infinity checks
- preset recall
