# VVChain

四段式音訊鏈結 VST3 / AAX 專案。

## Signal Flow

INPUT
→ Analog 4Band EQ + HF (40–120 Hz)
→ 4Band OTT-style parallel dynamics
→ A-Type nonlinear enhancer
→ Split-Band De-Esser
→ Dry/Wet Mix
→ Output Level
→ OUTPUT

## Current architecture

- JUCE 9.0.2 / C++20
- VST3 + Standalone
- AAX build switch guarded by \`VVCHAIN_ENABLE_AAX\`
- 4-band parametric EQ + adjustable HF/HPF corner 40–120 Hz
- 4-band OTT-style parallel dynamics with threshold, up/down ratios, attack/release, three crossovers, depth, mix and post gain
- A-Type nonlinear enhancer with amount, drive, bias, mix, tone and HPF
- Split-band de-esser with frequency, Q, threshold, range, attack, release and listen mode
- Dry/Wet and output level
- APVTS state save/restore
- ZL Equalizer-inspired interaction language, independently implemented without copying ZL branding/assets
- Native editor exposes module-specific detail controls
- Web preview at GitHub Pages for interactive UI / audio testing

The 40–120 Hz control is intentionally retained as the diagram's \`HF\` label, while the DSP treats it as a high-pass corner.

## Web Preview

The online preview supports:
- drag-and-drop audio loading
- waveform display
- mouse range selection
- sample-accurate Web Audio loop range via \`loopStart\` / \`loopEnd\`
- play / stop / bypass / reset
- draggable EQ nodes
- module-specific detailed parameter controls

The browser DSP is a preview-oriented implementation for interaction testing. The JUCE C++ DSP is the production implementation.

## Build

\`\`\`powershell
cmake -S . -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Release -DVVCHAIN_ENABLE_AAX=OFF
cmake --build Builds --config Release
\`\`\`

AAX additionally requires the legitimate AAX SDK/developer environment and release signing.

## Validation

The repository contains a deterministic reference stress test with exactly 2,055 cases:

- 280 planning / parameter-space cases
- 120 boundary / debug cases
- 180 all-feature parameter-sensitivity cases
- 655 transient cases
- 820 full-chain cases

Final validation must still include pluginval, REAPER/Cubase/Pro Tools loading, automation, state recall, mono/stereo, sample-rate and block-size changes, and AAX signing.
