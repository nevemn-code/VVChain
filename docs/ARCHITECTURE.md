# VVChain Architecture

Signal flow:

INPUT
→ 4-band per-band Analog Color EQ
→ 4-band OTT-style dynamics
→ 4-band TAPE-A dynamic enhancer
→ split-band De-Esser
→ dry/wet
→ output level
→ OUTPUT

## Parametric / Dynamic EQ

- Four EQ bands use a double-precision Cytomic/Simper TPT Bell topology.
- Bell damping follows k = 1 / (Q * A), with A = 10^(gain/40); no secondary empirical Q reduction is applied.
- The filter has no additional plugin sample latency; its state and coefficient math remain double precision.
- At 0 dB, the Bell mix coefficient is exactly zero, so the EQ path is structurally identity.
- Native and Web Preview use the same Bell topology and Q mapping.
## DSP layers

1. Parameter layer: JUCE AudioProcessorValueTreeState.
2. Processor layer: converts host parameters to one DSP parameter struct.
3. DSP layer: sample processing and persistent detector/filter/nonlinear state.
4. Editor layer: EQ response graph, Shared X-Over controls, TT/SS controls, module controls and bypass LEDs.
5. Validation layer: deterministic reference stress tests plus host/pluginval validation.

## Analog Color

Each EQ band has an independent Analog Color amount and TT/SS mode.

- TT: softer tanh/ADAA path with controlled even-order contribution.
- SS: steeper tanh/ADAA path emphasizing odd-order saturation.
- The implementation avoids a separate oversampling stage, so it does not add plugin latency.

## De-Esser

- Frequency: 6–18 kHz.
- Maximum Reduction: 0–8 dB, default 0 dB.
- When reduction is 0 dB, the realtime preview can bypass the block processor path.

## Release gate

Reference tests passing is necessary but not sufficient. Release requires compiled VST3 validation in real hosts, automation/state recall, sample-rate/block-size changes, mono/stereo, and AAX-specific validation/signing.
