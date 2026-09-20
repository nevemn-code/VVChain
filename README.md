# VVChain

四段式音訊鏈結 VST3 / AAX 專案。

## Signal Flow

INPUT
→ Analog 4Band EQ + HF (40–120 Hz)
→ 4Band OTT parallel compressor
→ 4Band A-Type Enhancer
→ De-Esser / Split-Band
→ Dry/Wet Mix
→ Output Level
→ OUTPUT

## Current architecture

- JUCE 9.0.2 / C++20
- VST3 + Standalone
- AAX build switch guarded by `VVCHAIN_ENABLE_AAX`
- 4-band parametric EQ
- HF/HP corner 40–120 Hz
- OTT-style parallel dynamics prototype
- A-Type nonlinear enhancer prototype
- Split-band de-esser prototype
- Dry/Wet and output level
- APVTS state save/restore
- ZL Equalizer-inspired interaction language, independently implemented

The 40–120 Hz control is intentionally retained as the diagram's `HF` label, while the DSP treats it as a high-pass corner.

## Build

```powershell
cmake -S . -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Release -DVVCHAIN_ENABLE_AAX=OFF
cmake --build Builds --config Release
```

AAX additionally requires the legitimate AAX SDK/developer environment and release signing.

## Validation

The repository contains a reference stress test, but passing reference simulations are not equivalent to DAW/pluginval/AAX certification. Final validation must include pluginval, REAPER/Cubase/Pro Tools loading, automation, state recall, mono/stereo, sample-rate and block-size changes, and AAX signing.

## UI

The first UI prototype uses a dark spectrum/editor layout inspired by the interaction language of ZL Equalizer, without copying its logos or brand assets.
