# VVChain

四段式音訊鏈結 VST3 / AAX 專案，主介面固定為單一 plugin 視窗。

## Signal Flow

INPUT
→ 4-Band Parametric EQ + Analog Color
→ 4-Band OTT
→ 4-Band TAPE-A
→ Two-Edge Split-Band De-Esser
→ MIX
→ OUT
→ OUTPUT

## Main UI

- 上方 EQ response + realtime FFT。
- 3 條可拖曳 Shared X-Over 線，分成 4 個頻段；線上滾輪調整 OVERLAP。
- BAND 1–4：FREQ / GAIN / Q / ANALOG COLOR / OTT % / ATTACK / RELEASE / TAPE-A +。
- 各 BAND 有獨立 OTT 與 TAPE-A BYPASS LED；亮 = 啟用，暗 = BYPASS。
- DE-ESSER 框內四顆旋鈕垂直排列：DE-ESS FREQ / DE-ESS % / MIX / OUT。
- DE-ESS FREQ 與 DE-ESS % 約為 MIX / OUT 旋鈕尺寸的 80%，仍保持同一垂直軸。
- DE-ESSER BYPASS 使用與其他模組一致的亮燈邏輯：亮 = 啟用，暗 = BYPASS。
- Master BYPASS 位於右上角，為全鏈旁通；啟用時 UI 灰階化。

## DSP

- Native DSP 使用固定 8192-sample De-Esser PDC。
- HP / CORNER 已移除，不再參與聲音計算。
- De-Esser 預設強度為 0%，預設不改變聲音。
- Master BYPASS 維持固定 PDC，切換使用短交叉淡化。
- Realtime FFT 在背景執行緒計算，GUI 只讀已平滑的頻譜資料。
- AAX 目標受 VVCHAIN_ENABLE_AAX 控制，需合法 AAX SDK / 開發環境。

## Web Preview

GitHub Pages 預覽支援：

- LOAD AUDIO：選取音檔後解碼，再 PLAY / STOP / RESET。
- 預設 De-Esser 強度為 0%，因此播放不需要先等 De-Esser 8192-sample block。
- 強度大於 0% 且未 BYPASS 時，才進入 8192-sample De-Esser block path。
- Master BYPASS 會切至原始輸入並將整個介面灰階化。
- Worklet 發生處理錯誤時，狀態列顯示 DSP ERROR。
- DE-ESSER 的 MIX / OUT 直接與 native UI 同樣放在 DE-ESSER 框內。

Online preview:
https://nevemn-code.github.io/VVChain/

## Native Build

- JUCE 9.0.2 / C++20
- VST3 + Standalone
- AAX switch guarded by VVCHAIN_ENABLE_AAX

## Validation

- Tests/reference_stress.py：DSP / 參數空間 deterministic regression。
- Tests/web_smoke.py：Web Preview JavaScript 語法、UI 結構、音檔載入 / 播放、BYPASS、DE-ESSER、MIX / OUT regression。

> Regression tests are not a substitute for final DAW pluginval or AAX certification.