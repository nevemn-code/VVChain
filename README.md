# VVChain

四段式音訊鏈結 VST3 / AAX 專案。

## Signal Flow

INPUT
→ Analog-Colored 4Band EQ + HF/HPF
→ 4Band OTT (Downward + Upward)
→ Type-A 4Band Dynamic Enhancer
→ Two-Edge Split-Band De-Esser
→ Dry/Wet Mix
→ Output Level
→ OUTPUT

## Module design

### EQ / ANALOG

- 4-band parametric manual EQ.
- 40–120 Hz HF/HPF corner.
- Each EQ band is followed by a nonlinear analog-style coloration stage.
- Analog Color is independently adjustable.
- Web interaction intentionally uses an ear-first/manual-search UI language inspired by flowEQ; its original DSP remains a clean biquad implementation, so VVChain does not use it as the coloration model. citeturn634172search0

### OTT

- Four independently adjustable OTT degrees: Band 1–4.
- Three crossover frequencies create four processing bands.
- Downward compression followed by upward compression per band.
- Shared threshold, upward ratio, downward ratio, attack and release.
- Input drive, post gain and wet/dry mix.
- The four-band architecture is based on the documented OTT signal flow of multiband crossover → downward compressor → upward compressor → band sum → depth mix. citeturn634172search1turn634172search3

### TYPE-A

Type-A follows the Dolby A encode-stage enhancer concept rather than a generic waveshaper exciter:

- Band 1: low-pass around 80 Hz.
- Band 2: 80 Hz–3 kHz.
- Band 3: high-pass around 3 kHz.
- Band 4: high-pass around 9 kHz.
- The upper bands overlap.
- Each band has an independently adjustable degree.
- Each band also has a gain trim.
- Attack / release and parallel mix are adjustable.
- The process boosts quieter band content dynamically instead of intentionally generating new harmonics. citeturn979382search0turn979382search1turn979382search17

### DE-ESSER

- Two crossover edges define the active sibilance band.
- Range sets maximum attenuation.
- Strength sets how strongly the detected energy is reduced.
- Attack / release are adjustable.
- Listen mode exposes the detected band.

## Web Preview

The GitHub Pages preview supports:

- drag-and-drop audio loading
- waveform display
- mouse range selection
- loopStart / loopEnd loop playback
- PLAY / STOP / LOOP / BYPASS / RESET
- four draggable EQ nodes
- button-gated OTT / TYPE-A / DE-ESSER detail panels
- custom AudioWorklet DSP preview

Online preview:
https://nevemn-code.github.io/VVChain/

## Native build

- JUCE 9.0.2 / C++20
- VST3 + Standalone
- AAX switch guarded by VVCHAIN_ENABLE_AAX
- APVTS state save/restore

AAX still requires the legitimate AAX SDK/developer environment and release signing.

## Validation matrix

Exactly 2,055 deterministic reference cases:

- 280 planning / parameter-space cases
- 120 boundary / debug cases
- 180 all-feature control-mapping cases
- 655 transient cases
- 820 full-chain cases

These reference tests are regression checks, not a substitute for final pluginval, DAW, or AAX certification.
