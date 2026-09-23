# VVChain 開發規則

## 1. JUCE / Web Preview 雙版本同步
任何功能、UI、操作邏輯、參數、DSP 或互動修改，只要該功能存在於 Web Preview，就必須同步修改兩套實作：

- JUCE Plugin：`Source/`
- GitHub Pages Web Preview：`docs/index.html`

不可只修改其中一邊就視為完成。

## 2. 每次改版的同步檢查
完成修改後，必須至少確認：

- 參數名稱、範圍、預設值一致
- 滑鼠拖曳方向與操作方式一致
- Frequency / Gain / Dynamics 等控制的事件路由一致
- 數值計算與曲線邏輯一致
- UI 顯示與即時數值同步
- BYPASS / RESET / 模式切換等互動一致
- 若是 DSP / 模擬 DSP 功能，兩邊的核心公式與映射一致

## 3. GitHub Pages 部署
Web Preview 由 `main` 分支的 `docs/` 部署。涉及 Web Preview 的修改必須提交到 `main`，並確認 Pages workflow 已被觸發。

## 4. 完成條件
除非明確指定功能只存在於 JUCE 或 Web Preview，否則：

**「Plugin 已修改」不等於完成；必須「Plugin + Web Preview 同步修改並完成測試」才算完成。**
