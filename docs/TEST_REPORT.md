# VVChain Validation Status（v1.0.49；測試基準仍為 v1.0.48）

This document records what the repository actually verifies. It is not a claim of DAW certification.

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
