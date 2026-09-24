# VVChain 專案規則入口

VVChain 的唯一正式開發規則位於：

`.github/VVCHAIN_RULES.md`

本檔不再複製另一份規則，避免兩份文件長期分岔。

目前必要原則：
- 每一批 GitHub 修改都遞增 PATCH 版本。
- Native VST3 與 Web Preview / AudioWorklet 的共同功能必須同步。
- ANALOG COLOR 正式基準為 v1.0.45 的 unity-normalized smooth algebraic saturation + analytical first-order ADAA。
- ANALOG COLOR 使用者處理範圍為 0–60%；TT/SS 維持 odd-symmetric，ADAA state 逐 band/channel 隔離；X2 只把 Analog delta ×2，不影響其他模組。
- 一般 PR 使用 Fast Gate；重型 Native / DSP 驗證改為手動 Full Validation。
- Web Pages 從 `main/docs` 獨立快速部署，不等待 Windows VST3 建置。
