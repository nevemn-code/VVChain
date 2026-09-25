# VVChain Architecture（v1.0.60）

> 以下以目前程式實際執行為準；本次封閉測試的失敗和限制見 `TEST_REPORT.md`。

Signal flow:

INPUT
→ 4-band Parametric / Dynamic EQ + per-band Analog Color
→ 4-band UDMBC
→ 4-band TAPE COLOR
→ dry/wet
→ output level
→ master limiter
→ OUTPUT

實際 Native 順序：EQ／Dynamic EQ（host rate）→ 四段 TRANSIENT（host-rate parallel delta）→ 四段 Analog Color（nonlinear stage 固定 4×）→ UDMBC → TAPE COLOR → Mix／Out → Solo → 4× true-peak limiter → 主 Bypass／Delta。TRANSIENT 與 Analog 共用 X1／X2／X3 頻段設定，但 TRANSIENT 不重建整條 split signal；只注入各頻段的 gain delta，TRANSIENT 先於 Analog。

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
- ADAA 的 previous sample 依頻段和聲道獨立保存；每段 alpha 使用約 0.25 ms smoothing。四段 Color 全 0%／Bypass 時完全跳過 4× up/downsample，以等延遲 pure-delay path 維持固定 PDC；Linear EQ / Dynamic EQ 不進入 Analog oversampling domain。

## 參數與操作

- LF／HF ROLL-OFF 的 OCT 離散選項是 6／12／24／36／48／60／72 dB/oct；預設 12。Band 2／3 的舊 roll-off preset 會映射至 Bell；其他已移除類型也保留舊參數槽。
- Dynamic EQ 各段有 Target、DYNAMICS（−100% 至 +100%，0% 中心）、Attack／Release、偵測比例及 Mid／Side 權重。
- UDMBC 四段有獨立 gate／upward／downward 狀態。TAPE COLOR 的舊 Attack／Release 參數保留作 preset 相容，目前 tanh 染色增益不使用它們。
- Solo 可取處理前或處理後的頻段／頻率，位置在最終 limiter 前；Delta 取最終輸出與對齊總延遲的原聲之差。

## TRANSIENT

- 四段 bipolar amount：-100%～+100%，0% 為 true zero-work bypass。
- Stereo detector 使用 squared energy，不做 sqrt；Fast/Slow squared envelope 以一次 log-ratio 產生 transient control。
- Band 1 的 70 Hz / 12 dB/oct HPF 僅在 detector sidechain，audible signal 不經該 HPF。
- Rational soft-knee 使用 x/(1+|x|)，最大控制增益 12 dB 為漸近安全上限；最後增益做極短 smoothing。
- Native TRANSIENT 在 host rate 以獨立 detector/filter state 取得四段 band signal，但 audible base path 保持原樣，只注入 (gain−1)×band；因此 0% exact bypass，啟用時也不增加整條 full-band split/recombine phase rotation。
- TRANSIENT 位於 Analog 前，讓後續 Analog / UDMBC / TYPE-A 接住被強化或削弱的 attack。

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


## DELTA analyzer source switching (v1.0.57)

Normal Analyzer mode remains a pre-DSP/original spectrum reference plus the three module contribution overlays. When `DELTA_MONITOR` is ON, the main Analyzer source changes to the actual final Delta monitor signal after the normal output path has already performed `processed - latency-aligned dry`. The original input spectrum is not drawn in this mode.

Module contribution overlays are disabled and cleared while DELTA is active, because the audible Delta spectrum already represents the total difference signal and stacking per-module added-energy overlays on top would mix two incompatible display meanings.

Native performs this switch by writing the Analyzer FIFO before DSP only when DELTA is OFF, and after `dsp.process()` only when DELTA is ON. Web switches the AnalyserNode connection from `source` to the AudioWorklet output. Turning DELTA OFF reconnects the original analyzer reference automatically.


## v1.0.59 CPU / Analyzer architecture

- Main Spectrum Analyzer only: 4096-point Hann FFT.
- ANALOG / UDMBC / TYPE-A contribution analyzers, six pre/post streams and Web Worklet contribution messages are removed.
- DYNAMICS=0% uses a static coefficient path; active Dynamic EQ retains the realtime detector/gain path.
- UDMBC and Type-A return before crossover/detector/waveshaper work when every band is effectively inactive.
