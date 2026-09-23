# VVChain

四段式音訊鏈結 VST3 / AAX 專案，主介面固定為單一 plugin 視窗。

## Signal Flow
INPUT
→ 4-Band Parametric EQ + per-band Analog Color
→ 4-Band OTT
→ 4-Band TAPE-A
→ Two-Edge Split-Band De-Esser
→ MIX
→ OUT
→ OUTPUT

## Main UI
- 上方顯示 EQ response 與 Shared X-Over，不再使用即時 FFT analyzer。
- 3 條可拖曳 Shared X-Over 線，分成 4 個頻段；線上滾輪調整 OVERLAP。
- BAND 1–4：FREQ / GAIN / Q / ANALOG COLOR / OTT % / ATTACK / RELEASE / TAPE-A +。
- 每個頻段的 ANALOG COLOR 完全獨立；0% = 區域透明，100% = 約 30% 透明。
- ANALOG COLOR 上方有 TT / SS 撥桿：TT = Tube Saturation；SS = Solid-State Saturation。
- 各 BAND 的 OTT / TAPE-A BYPASS 小燈固定位於對應旋鈕右上方；亮 = 啟用，暗 = BYPASS。
- DE-ESSER 框內四顆旋鈕垂直排列：DE-ESS FREQ / MAXIMUM REDUCTION / MIX / OUT。
- MAXIMUM REDUCTION 範圍 0–8 dB，預設 0 dB；DE-ESSER 預設不改變聲音。
- Master BYPASS 位於右上角，為全鏈旁通；啟用時 UI 灰階化。
- +ADV 開啟後，點視窗外即可關閉。

## DSP
- Native De-Esser 使用固定 8192-sample PDC。
- HP / CORNER 不參與聲音計算。
- ANALOG COLOR 採「乾聲基波 1:1 + 加法諧波」結構：15 ms RMS 只用來正規化染色諧波的量級，不形成壓縮增益；TT 主要為偶次諧波、SS 主要為奇次諧波，避免整段訊號進入 tanh 而產生壓縮感。
- Master BYPASS 保持固定 PDC，完全旁通時輸出延遲乾聲。
- AAX 目標受 VVCHAIN_ENABLE_AAX 控制，需合法 AAX SDK / 開發環境。

## Web Preview
- LOAD AUDIO：選取音檔後解碼，再 PLAY / STOP / RESET。
- Master BYPASS 會切至原始輸入並將整個介面灰階化。
- DE-ESSER 強度為 0 dB 時直接跳過其 8192-sample block path；大於 0 dB 才啟用。
- Worklet 發生處理錯誤時，狀態列顯示 DSP ERROR。
- DE-ESSER 的 MIX / OUT 與 Native UI 同樣放在 DE-ESSER 框內。

Online preview:
https://nevemn-code.github.io/VVChain/

## Native Build
- JUCE 9.0.2 / C++20
- VST3 + Standalone
- AAX switch guarded by VVCHAIN_ENABLE_AAX

## 版本日誌

### v1.0.1｜穩定基準版
- 已確認 Web 播放音訊處理鏈、參數控制、DELTA 與 Bypass 正常。
- 此版本作為後續 UI 修改的基準，不回改其已驗證的音訊處理鏈。

### v1.0.3｜Dynamic EQ / Graph 操作修正
- 修正 DYNAMICS 與該頻段靜態 GAIN 重複計算造成的高增益／爆音問題；Native 與 Web 都限制動態總 GAIN 在安全範圍。
- 上方 EQ / DYNAMICS / Q 操作統一顯示即時小框，列出 FREQ / GAIN / DYN / Q，正在移動的參數粗體化，滑鼠放開立即關閉。
- DYNAMICS Target 點改為可直接抓取控制；上下箭頭保留為獨立 DYNAMICS 微調把手。
- CI/CD 修正 JUCE `StringArray` 編譯陷阱與同步檢查範圍，並更新 GitHub Actions cache 版本。

### v1.0.2｜本次 UI / 操作更新
- 上方 EQ Graph 補回 dB 正負刻度與頻率刻度。
- 上方 DYNAMICS Target 點支援上下調整 DYNAMICS、左右同步移動 EQ 頻率；下方 DYNAMICS 與之同步。
- DE-ESSER 在 MAXIMUM REDUCTION 右上方增加獨立 BYPASS 控制，與原 DE-ESSER BYPASS 同一參數。
- OTT = 0、ANALOG COLOR = 0、TAPE-A + = 0、DE-ESSER MAXIMUM REDUCTION = 0 的灰階規則已加入。
- 第 4～7 項為可獨立撤回的視覺規則，保留後續回改空間。

## GitHub 開發規則
- 強制規則文件：`.github/VVCHAIN_RULES.md`
- 任何 `docs/*.html` 修改，都必須同步更新頁面版本號；不使用日期／時間碼作為版本識別。
- ANALOG 正式架構固定為 4-band V3 CHEBYSHEV；TT/SS 各段獨立，不得合併成單一全頻處理。

## Validation
- Tests/reference_stress.py：DSP / 參數空間 deterministic regression，包含 500 組 TT/SS 與 1,200 組頻率／振幅染色掃描，共 3,685 組案例。
- Tests/web_smoke.py：Web Preview JavaScript 語法、UI 結構、音檔載入 / 播放、BYPASS、DE-ESSER、MIX / OUT regression。

> Regression tests are not a substitute for final DAW pluginval or AAX certification.
  
### Analog Color / TT / SS
Analog Color is an additive harmonic-colour stage rather than a full-signal tanh compressor. TT (Tube Saturation) primarily injects controlled 2nd/4th-order harmonics with a very small 3rd-order component; SS (Solid-State Saturation) primarily injects controlled 3rd/5th/7th-order harmonics. The original waveform path remains at unity. A 15 ms RMS tracker normalizes only the added harmonic generator, so the stage is designed for colour rather than compression.

PSP's published ClassicQ documentation describes SIM as Class-A plus transformer simulation, with the Class-A stage before output level and the transformer followed by SAT; PSP does not publish the proprietary transfer curve. VVChain therefore uses that documented topology as a design reference rather than claiming a code-level clone. PSP describes its analog EQ/preamp coloration as gentle/subtle, and McQ describes SAT as a smooth overdrive stage.


## 開發同步規則（重要）
- **任何功能、UI、操作邏輯、參數、DSP 或互動修改，都必須同步檢查 JUCE Plugin 版與 GitHub Pages Web Preview。**
- JUCE 實作主要位於 `Source/`；GitHub Pages 實際執行版本位於 `docs/index.html`。
- **不能只修改 `Source/PluginEditor.cpp` / `ChainDSP.cpp` 就視為完成。** 若該功能在 Web Preview 存在，必須同步修改 `docs/index.html` 對應的 JavaScript / UI / DSP 模擬邏輯。
- 每次改版完成後，必須做「Plugin ↔ Web Preview」雙版本功能對照，至少確認參數範圍、拖曳方向、滑鼠事件、數值計算、UI 顯示與預設值一致。
- GitHub Pages 會從 `main` 的 `docs/` 部署；因此 Web Preview 的修正也必須直接提交到 `main`，並確認 Pages deployment 已觸發。
- **除非明確說明某功能只存在於其中一個版本，否則一律以雙版本同步為完成條件。**
