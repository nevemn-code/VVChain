# VVChain Architecture（v1.0.49）

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
