# VVChain Test Plan

## Default Fast Gate

Runs on pull requests and is intentionally lightweight:

- Source ↔ Web Preview synchronization rule.
- Version-only rule.
- Web AudioWorklet JavaScript syntax.
- Web smoke regression.
- Dynamic EQ / UI / interaction regression.
- Current Web visible version ↔ Worklet cache version parity.

The Fast Gate must not install the full Linux audio/X11 toolchain or rebuild Native VST3.

## Main push

- GitHub Pages deploys independently from `docs/`.
- Windows builds only the `VVChain_VST3` target.
- The PR Fast Gate is not repeated after merge.
- The Windows build reuses a stable incremental JUCE/MSVC build cache and does not copy the plugin into the runner's local plugin folder.

## Manual Full Validation

Manual `full_validation=true` is reserved for expensive checks:

- Linux VST3/DSP build.
- Analog 500-case matrix.
- DSP stress test.
- Additional host/pluginval checks when available.

## Current architecture checks

- Four Parametric / Dynamic EQ bands.
- Shared X1 / X2 / X3 crossover boundaries.
- Four independent UDMBC bands and bypass states.
- Four TAPE-A bands sharing the crossover ranges.
- Four independent Analog Color bands, 0–60 processing range, TT/SS, bypass and X2 delta ×2.
- De-Esser 6–18 kHz, 0–8 dB maximum reduction, four response modes.
- Compact two-line EQ / DYN EQ graph readout.
- Master bypass / dry path latency alignment.

## Host validation before public release

Still required when preparing a distributable release:

- pluginval
- target DAWs
- mono / stereo
- sample-rate and block-size changes
- automation and state recall
- bypass / dry-wet / Delta
- NaN / infinity / denormal behavior
- AAX SDK build/signing when AAX is enabled
