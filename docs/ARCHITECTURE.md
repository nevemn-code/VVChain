# VVChain Architecture（v1.0.56）

> 以下以目前程式實際執行為準；本次封閉測試的失敗和限制見 `TEST_REPORT.md`。

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

實際 Native 順序：EQ／Dynamic EQ 與四段 Analog Color（EQ 4× oversampling）→ UDMBC → TAPE COLOR → De-Esser → Mix／Out → Solo → 4× true-peak limiter → 主 Bypass／Delta。X1／X2／X3 與 OVERLAP 供 Analog、UDMBC、TAPE COLOR 及頻段 Solo 使用；四個 EQ 的頻率另由各自 FREQ 設定。

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
- 靜態 transfer 在 shaping domain 的 ±1 歸一化；ADAA 是有狀態的一階差分，沒有逐 sample 的 hard no-shrink guard。
- X2 doubles only the generated Analog delta; it does not multiply EQ / UDMBC / TAPE COLOR / De-Esser / Mix / Out.
- ADAA 的 previous sample 依頻段和聲道獨立保存；每段 alpha 使用約 0.25 ms smoothing。即使 Color 為 0，整個 EQ 路徑仍經過分頻重建及 oversampling，不能把整鏈宣稱為 bit-exact dry。

## 參數與操作

- LF／HF ROLL-OFF 的 OCT 離散選項是 6／12／24／36／48／60／72 dB/oct；預設 12。Band 2／3 的舊 roll-off preset 會映射至 Bell；其他已移除類型也保留舊參數槽。
- Dynamic EQ 各段有 Target、DYNAMICS（−100% 至 +100%，0% 中心）、Attack／Release、偵測比例及 Mid／Side 權重。
- UDMBC 四段有獨立 gate／upward／downward 狀態。TAPE COLOR 的舊 Attack／Release 參數保留作 preset 相容，目前 tanh 染色增益不使用它們。
- Solo 可取處理前或處理後的頻段／頻率，位置在最終 limiter 前；Delta 取最終輸出與對齊總延遲的原聲之差。

## De-Esser

- Frequency: 6–18 kHz.
- Maximum Reduction: 0–8 dB.
- Four response presets control attack / release / ratio.
- Current Native implementation is sample-domain split-band processing: low band passes untouched, only the high band is gain-reduced.
- The current De-Esser does not use the old 8192-sample FFT/block design and does not add an 8192-sample PDC by itself.

## Plugin latency

The reported plugin latency is derived from the active EQ oversampling path plus limiter oversampling/lookahead. Master bypass keeps the dry path aligned to the same reported latency.

Native lookahead 約 3 ms。`process` 若收到比 `prepareToPlay` 配置更大的 block，目前直接 return，屬待驗證風險。Web DSP 在 `docs/vvchain-worklet.js`，以實際 Worklet rate 執行；Native Analog 在 4× oversampling 內，兩端不能宣稱逐 sample 完全一致。

## Layers

1. APVTS parameter layer.
2. Processor parameter mapping.
3. Persistent DSP state and processing.
4. Native editor / Web Preview interaction layer.
5. Fast regression gate plus optional full Native / DSP validation.

## Release gate

A fast PR gate protects Source/Web synchronization, JavaScript syntax and interaction regressions. Windows VST3 compilation runs on main push. Heavy Linux Native / Analog / stress validation remains manual.


## Settings overlay (v1.0.50)

- Native JUCE and Web Preview both append a vector SETTINGS gear after the existing DE-ESS status area.
- The overlay is UI-only: opening/closing it never writes APVTS, never sends automation, never changes DSP state, latency, plugin bounds, or audio.
- Panel geometry is 330 px wide and at most 480 px high. Overflow scrolls inside the panel only.
- INTERFACE, CONTROL, ANALYZER / GRAPH and SYSTEM / ABOUT are scaffolded. AUDIO / QUALITY is reserved only.
- Any future behavior-affecting setting requires explicit persistence design before it can be enabled. Audio settings belong in APVTS only when host automation is actually required; UI preferences must remain outside APVTS.


## UI Theme (v1.0.52)

- Factory/default theme remains DARK.
- SETTINGS / INTERFACE exposes DARK / IVORY. This is a UI-only preference path and is intentionally outside APVTS.
- IVORY changes colours only: no bounds, hit areas, parameter values, DSP state, processing order, latency, or automation behavior may change.
- Native uses the editor/theme-aware LookAndFeel and paint paths; Web uses the `.ivoryTheme` class plus theme-aware EQ canvas colours.
- v1.0.52 is a visual trial build; theme persistence across a newly created plugin instance is intentionally not enabled yet.


## Spectrum Analyzer (v1.0.53)

Native analysis path is intentionally outside the audio processing chain:

`input tap -> lock-free FIFO -> 2048 Hann FFT -> 220 log-frequency display points -> cached JUCE Path`.

The audio callback only copies a mono analysis tap while Analyzer is enabled. FFT and path generation happen on the editor timer, so no FFT, allocation, drawing, or locks are added to the DSP path. Analyzer OFF disables FIFO writes and clears the display.

Web Preview keeps the production audio path unchanged. A parallel source branch feeds a 2048-point AnalyserNode and zero-gain sink only while Analyzer is enabled. This branch is disconnected when disabled. Analyzer settings remain UI-only and outside APVTS / host automation.


## Spectrum smoothing (v1.0.54)

The analyzer display now separates measurement smoothing from curve rendering:

1. 4096-point Hann FFT.
2. 75% overlap / 1024-sample hop.
3. Log-frequency 1/12-octave RMS-energy aggregation, with a minimum three-bin window at low frequencies.
4. Five-point Gaussian smoothing in display space.
5. Asymmetric temporal EMA (fast attack, slower release).
6. 4.5 dB/oct display tilt around 1 kHz.
7. Catmull-Rom converted to cubic Bezier for the final path.

These steps affect only metering/visualization. Audio samples are never reconstructed from the smoothed spectrum and DSP output remains unchanged.


## Pro-style main analyzer + module contribution layers (v1.0.55)

Main spectrum is display-only and uses 4096-point Hann FFT, 50% overlap, 256 logarithmic display points, power-domain fractional-octave averaging, seven-tap binomial smoothing, asymmetric time ballistics, 4.5 dB/oct tilt around 1 kHz and monotone cubic Hermite rendering.

Three separate contribution layers identify the module that created added spectral content:

- ANALOG: orange/gold.
- UDMBC: cyan/blue.
- TYPE-A: purple/pink.

For each module the analyzer taps Pre and Post signals and derives the spectral shape from `Delta = Post - Pre`. The overlay is ADDED-only: it grows upward only where Post power is greater than Pre power and the Post spectrum is above the silence gate. This prevents compression/removal Delta from being drawn as added energy.

The three layers stack from the current main-spectrum edge in processing order ANALOG -> UDMBC -> TYPE-A. Each module is capped at 8 dB of visual growth; combined growth is normalized to a 12 dB visual maximum. These numbers are visualization scaling, not absolute dBFS output readings.

Contribution analysis uses a separate 2048-point Hann FFT path with lighter frequency smoothing and faster ballistics so harmonics remain visible. Native carries six synchronized mono analysis streams (Analog pre/post, UDMBC pre/post, Type-A pre/post). Analog's pre-tap is converted from the existing 4x EQ/Analog domain by an analyzer-only companion downsampler; that signal never feeds the audible chain.

Analyzer collection is disabled when the Analyzer is OFF or the Native editor is not showing. Web also suspends contribution traffic when the page is hidden. None of these analyzer taps write APVTS, host automation, latency, or audible samples.


## Analyzer safety refinements (v1.0.56)

Master BYPASS suppresses and clears module contribution layers so stale pre-bypass Delta data is never left on screen. Contribution FIFO copies are bounded by the preallocated analyzer stream size; analyzer metering may drop excess analysis samples from an abnormal oversized host block, but it must never enlarge or alter the audible processing buffer.
