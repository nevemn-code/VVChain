# VVChain Validation Status（v1.0.60；歷史基準仍保留於下文）

## v1.0.60 Transient implementation
- Branch: `feature/transient-v1.0.60`.
- De-Esser realtime DSP / APVTS / dedicated Native UI / Web UI and Worklet processing removed.
- Four-band TRANSIENT added before Analog. Native now runs it at host rate with parallel-delta injection using the same X1/X2/X3 frequency definitions; the untouched base path remains intact, so Transient does not add a full-band crossover phase rotation or wake Analog 4× by itself.
- Stereo-linked squared-energy detector, detector-only Band 1 70 Hz HPF, single log-ratio and rational soft-knee implemented.
- CI / VST3 validation status is recorded by the PR workflow; host listening and pluginval remain separate host validation.

## v1.0.59 CPU architecture change

- Linear EQ / Dynamic EQ 已移出 Analog 4× domain；Analog ADAA v2 啟用時仍固定 4×。
- Analog 全 0 / bypass 時不執行 oversampler，使用等 PDC delay；此項仍需要 Windows VST3 / DAW null、automation transition 與 latency 實測確認。
- 模組 Contribution Analyzer 已自 Native / Web runtime 移除，只保留主 Spectrum Analyzer。
- DYNAMICS=0、UDMBC 全 0、Type-A 全 0 的 lazy paths 已加入 source regression guards。
- 本文件不把 source-level regression 當成 DAW 實測；最終聲音等同性仍需 artifact + host 驗證。


This document records what the repository actually verifies. It is not a claim of DAW certification.

## v1.0.50 UI-only Settings scaffold

本版新增 Native / Web SETTINGS 齒輪與 overlay 容器，不修改 DSP。所有尚未完成 persistence / behavior 定義的設定均標成 RESERVED / DISABLED；AUDIO / QUALITY 沒有接入任何音訊參數。開啟／關閉 overlay 的程式路徑不呼叫 APVTS parameter write，也不向 Web AudioWorklet sendParams。

既有五個測試腳本在 v1.0.49 已記錄有前置 source-guard / Python syntax 問題；本版不把那些既有失敗誤報成 Settings UI 已通過完整 CI。Windows VST3 / DAW / pluginval 仍需由 Actions artifact 與實際 host 驗證。

v1.0.51 另修正 Web visible version 與 AudioWorklet cache query parity；此修正不改 DSP。

v1.0.52 新增 DARK / IVORY 純 UI theme。切換路徑不寫 APVTS，也不向 Web AudioWorklet 傳送參數；本版不宣稱改動任何聲音結果。


## 十輪本機封閉測試

基準為 `main` commit `ad9a8e6`。以相同五個腳本各重跑十輪，共 50 次執行；十輪結果相同。重複執行檢查穩定性，不等於十種音訊輸入情境。

| 腳本 | 通過／10 | 最先發現的問題 |
| --- | ---: | --- |
| `project_static_audit.py` | 0 | 第 44 行硬找規則文字 `X1/X2/X3`，提前 AssertionError。 |
| `web_smoke.py` | 0 | 第 82 行 `"zoneBands(y,c,"analogLp",s.udmbc.x)"` 引號沒有跳脫，Python SyntaxError；PR Fast Gate 因此必敗。 |
| `dynamic_eq_ui.py` | 0 | 第 115 行要求舊註解 `Direct DYNAMICS target control takes priority`，提前 AssertionError。 |
| `reference_stress.py --iterations 50` | 0 | `analogADAA[band][ch].processSample` 已換行並改索引形式，source guard 提前失敗，迭代未開始。 |
| `analog_matrix.py` | 0 | `.cpp` 搜尋 `#include "VVChain_AnalogADAA_v2.h"`，include 實際在 `.h`，500 cases 未開始。 |

另以 `node --check docs/vvchain-worklet.js` 確認 Worklet 語法可解析。Pages 獨立的 JS 語法檢查與 PR Fast Gate 不同；Web 能部署不能視作整套 CI 已通過。本次未建置 Windows VST3，也未進行 DAW、pluginval 或實際音訊 null 測試。

## 待驗證的程式風險

- `ChainDSP.cpp::process` 收到比 `prepareToPlay` 預配區塊更大的 buffer 時直接 return，未執行固定延遲處理。
- Analog 0% 時非線性增量為零，但程式仍執行 EQ oversampling 與分頻重建；需測量完整路徑的 null／相位。
- Native Analog 位於 EQ 4× oversampling，Web 在 Worklet rate 運作；沒有逐 sample 同一性量測。

上列 source guard 失敗屬測試或 CI 問題，不能單憑它們判定音訊 DSP 已破音、無聲或失去相位。本次保留程式現狀，只更新文件並列出錯誤。

## Automated PR gate

Fast gate 設計上檢查（目前因上述錯誤未通過）：

- Native/Web source synchronization.
- Web and AudioWorklet JavaScript syntax.
- Web smoke invariants.
- Dynamic EQ / graph / bypass / interaction regressions.
- Compact EQ / DYN EQ floating readout behavior.
- Shared continuous Q-wheel behavior.

## Manual full validation

手動路徑設計為建置 Native VST3，並執行 Analog 及 DSP reference 測試；現有腳本的前置斷言需修正。

## Known validation boundary

Repository source tests do not replace real host validation. Windows VST3 compilation, pluginval, target DAWs, automation/state recall, mono/stereo, sample-rate changes and AAX signing remain separate release checks.

## Historical TPT Bell reference

The v1.0.9 independent reference run recorded very small transfer-function differences against its comparison implementation and no NaN/Inf in its modulation stress cases. Those figures are historical reference data, not a claim that every later release automatically reran the same external comparison.


## v1.0.53 Analyzer / Web blank-page fix

v1.0.52 的 Pages workflow 雖通過 JavaScript syntax gate，但 Theme 初始化在 `state` 建立前呼叫 `drawEQ()`，屬於 runtime ordering error，因此瀏覽器會中止後續 UI 初始化。v1.0.53 移除該 early draw，Theme click 才在完整 state 建立後 redraw。

Analyzer 新增的 Native FIFO tap / FFT 與 Web AnalyserNode branch 均為視覺分析用途，不屬於聲音處理鏈。Windows VST3 build 與 Pages runtime/deployment 仍以 Actions 結果為最終驗證。


## v1.0.54 Analyzer smoothness / Native compile fix

v1.0.53 Pages deployment passed, but Windows VST3 failed because Analyzer editor state was accidentally declared inside the nested MetalLookAndFeel class. v1.0.54 moves that state to VVChainAudioProcessorEditor and simultaneously upgrades visual smoothing to 4096 FFT, 75% overlap, fractional-octave RMS averaging, Gaussian display smoothing, asymmetric time smoothing and cubic rendering.

The analyzer changes remain visualization-only; no DSP formula or audio parameter is changed.


## v1.0.55 validation target

本版修復先前已知的 `web_smoke.py` 引號語法錯誤、`dynamic_eq_ui.py` 的固定 v1.0.44 斷言與過時註解 guard，並讓 Fast Gate 在 main push 實際執行。CI 配置要求 Web smoke ×10、whole-project static audit ×10、UI/interaction regression ×10，再與 Windows VST3 build 和 Pages deployment 分別驗證。

Analyzer 本版新增的 pre/post taps、Delta FFT 與彩色 contribution layer 均為 metering 支線。Native Analog 的 pre-tap 使用獨立 analyzer-only downsampling path，結果只送往 analyzer FIFO；不回寫 `buffer`。因此設計意圖是不改變 DSP / PDC，但是否成功編譯及所有 source guards 是否通過，必須以 v1.0.55 Actions 實際結果為準。

DAW、pluginval、實際 CPU profiler、AAX 簽署與跨 sample-rate host 實測仍不由 source regression 取代。


## v1.0.55 Fast Gate result / v1.0.56 fix

v1.0.55 Pages deployment passed. Fast Gate reached the Web smoke step but stopped on a pre-existing Python syntax defect in the forbidden-files loop (`for` keyword missing), so later x10 gates were correctly skipped rather than falsely reported as passing. v1.0.56 repairs that test syntax and removes a stale project-audit assertion that checked only the literal text `X1/X2/X3` in the rules document rather than the real DSP crossover invariants.

The main analyzer/contribution calculation and color rules are unchanged from v1.0.55. v1.0.56 also clears contribution layers during Master BYPASS and bounds contribution FIFO copies to the preallocated analyzer-stream size, preventing stale overlays and analysis-buffer overrun in an oversized host block.


## #1254 result and v1.0.57 follow-up

VVChain Fast CI/CD #1254 completed with overall failure because the Fast Gate stopped in `JavaScript / Web smoke x10` on an outdated source assertion. The Windows VST3 Release job itself passed, and the corresponding Pages deployment passed. v1.0.57 updates the stale smoke/static/UI guards and adds explicit Native/Web regression contracts for DELTA Analyzer source switching.

The DELTA change is visualization routing only: the audio Delta formula remains `processed output - latency-aligned dry`; v1.0.57 only changes which signal the Spectrum Analyzer observes while DELTA is active.


## #1254 / #1255 CI status and v1.0.58

#1254 failed in Fast Gate before the requested repeated validation could complete. #1255 then passed Web smoke x10 and whole-project static audit x10, but stopped in UI / interaction regression because the test still expected the obsolete `resetDynamics` source token. The actual UI reset implementation is `resetParameter("DYN_DYNAMICS" + n, 0.0f)`.

v1.0.58 updates that source guard only; DELTA Analyzer routing remains the v1.0.57 implementation: when DELTA is ON, the main spectrum source is the actual final Delta output and the original/reference spectrum plus module contribution overlays are suppressed.
