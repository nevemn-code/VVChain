# VVChain 專案規則入口

VVChain 的唯一正式開發規則位於：

`.github/VVCHAIN_RULES.md`

本檔不再複製另一份規則，避免兩份文件長期分岔。

目前必要原則：
- 每一批 GitHub 修改都遞增 PATCH 版本。
- Native VST3 與 Web Preview / AudioWorklet 的共同功能必須同步；shared crossover 使用 LR4，UDMBC 使用 stereo-linked detector，Type-A 使用同一 analytical ADAA transfer。
- ANALOG COLOR 正式基準為 v1.0.47 的 unity-normalized smooth algebraic saturation + analytical first-order ADAA。
- ANALOG COLOR 使用者處理範圍為 0–60%；TT/SS 維持 odd-symmetric，ADAA state 逐 band/channel 隔離；X2 只把 Analog delta ×2，不影響其他模組。0% nonlinear delta 為零；neutral full-chain / DELTA 必須由 headless null test 驗證。
- 一般 PR 使用 Fast Gate；重型 Native / DSP 驗證改為手動 Full Validation。
- Web Pages 從 `main/docs` 獨立快速部署，不等待 Windows VST3 建置。
- v1.0.48 基準的封閉測試結果與失效位置見 `docs/TEST_REPORT.md`；CI 規則不能視為已通過的驗證。
