# VVChain Architecture

Signal flow:

INPUT
→ 4-band Parametric / Dynamic EQ + per-band Analog Color
→ 4-band UDMBC
→ 4-band TAPE COLOR
→ split-band De-Esser
→ dry/wet
→ output level
→ master limiter
→ OUTPUT

## Parametric / Dynamic EQ

- Four EQ bands use the current double-precision TPT Bell topology.
- Bell damping follows `k = 1 / (Q * A)`, with `A = 10^(gain/40)`.
- Static EQ, Dynamic EQ and the response graph share the same Q / Bell mapping.
- At 0 dB static gain the static Bell contribution is structurally neutral.
- Native and Web Preview must keep the same parameter ranges and response mapping.

## Analog Color

Each EQ band has an independent Analog Color amount, TT/SS mode, bypass and X2.

- User processing range: 0–60.
- 0 = exact dry.
- Core transfer: unity-normalized smooth algebraic saturation based on `x / (1 + alpha*x^2)^(1/4)`.
- A hard no-shrink guard prevents the shaping domain from reducing sample magnitude.
- X2 doubles only the generated Analog delta; it does not multiply EQ / UDMBC / TAPE COLOR / De-Esser / Mix / Out.
- The current Analog core is stateless and adds no filter phase rotation.

## De-Esser

- Frequency: 6–18 kHz.
- Maximum Reduction: 0–8 dB.
- Four response presets control attack / release / ratio.
- Current Native implementation is sample-domain split-band processing: low band passes untouched, only the high band is gain-reduced.
- The current De-Esser does not use the old 8192-sample FFT/block design and does not add an 8192-sample PDC by itself.

## Plugin latency

The reported plugin latency is derived from the active EQ oversampling path plus limiter oversampling/lookahead. Master bypass keeps the dry path aligned to the same reported latency.

## Layers

1. APVTS parameter layer.
2. Processor parameter mapping.
3. Persistent DSP state and processing.
4. Native editor / Web Preview interaction layer.
5. Fast regression gate plus optional full Native / DSP validation.

## Release gate

A fast PR gate protects Source/Web synchronization, JavaScript syntax and interaction regressions. Windows VST3 compilation runs on main push. Heavy Linux Native / Analog / stress validation remains manual.
