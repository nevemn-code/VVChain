# VVChain Architecture

Signal flow:

INPUT
→ 4-band Analog-style EQ + 40–120 Hz HP corner
→ 4-band OTT-style parallel dynamics
→ A-Type nonlinear enhancer
→ split-band de-esser
→ dry/wet
→ output level
→ OUTPUT

## DSP layers

1. Parameter layer: JUCE AudioProcessorValueTreeState.
2. Processor layer: converts host parameters to a single DSP parameter struct.
3. DSP layer: sample processing and persistent detector/filter state.
4. Editor layer: spectrum-style visualization and module controls.
5. Validation layer: deterministic reference stress tests plus future host/pluginval tests.

## Release gate

Reference tests passing is necessary but not sufficient. Release requires compiled VST3 validation in real hosts, automation/state recall, sample-rate/block-size changes, mono/stereo, and AAX-specific validation/signing.
