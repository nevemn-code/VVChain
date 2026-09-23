# VVChain GitHub 開發規則

## 版本規則（最高優先）

VVChain 所有 GitHub 版本追蹤統一使用「版本號」，不再使用任何時間碼。

### 版本號格式

Commit / Release / PR / Issue / CI/CD / Web Preview 等需要標示版本的內容，使用：

`[vMAJOR.MINOR.PATCH]`

例如：

`[v1.0.0] DSP: update OTT smoothing`

版本號只代表程式版本，不代表日期或電腦時間。

### 強制規則

1. 不得要求使用者提供、比對、同步或確認電腦時間。
2. 不讀取使用者電腦系統時間來建立版本識別。
3. 不在 Commit message、PR、Issue、Review、Workflow、Web Preview 中加入 TST / TIMECODE / LAST MODIFIED 等時間欄位。
4. 每次可發版的功能修改，版本號至少遞增 PATCH；重大不相容變更才遞增 MAJOR / MINOR。
5. 同一批連續修正可使用同一版本號，直到該版本完成。
6. Web Preview 不需要因程式修改而同步更新日期或時間顯示。
7. GitHub 自己產生的建立時間、Push 時間、Workflow 時間屬於 GitHub 平台資料，不納入 VVChain 版本規則。

### 版本驗收

每次改版確認：
- Commit 有版本號。
- 需要對外識別的 Web Preview / Release 有對應版本號。
- 不再使用舊的時間碼規則。
- Native VST3 與 Web Preview 仍遵守雙版本同步規則。

## ANALOG 規則

目前正式主版本使用 V3 CHEBYSHEV。

- ANALOG 必須維持 4 個獨立頻段。
- 每個頻段有自己的 ANALOG COLOR。
- TT 與 SS 必須是各自獨立的演算法設定，不得平均、合併或共用。
- 每個頻段的 TT/SS 選擇必須只影響該頻段。
- 不得把四段 ANALOG 改成單一全頻 ANALOG。
- 修改 ANALOG 後，至少執行既有 500-case regression matrix。

## OTT 規則

- OTT 四頻段必須維持獨立的頻段 / 聲道狀態。
- `currentEnv` 與 `currentGain` 必須放在 class state，不得在 process 迴圈內每次重新初始化。
- Low 頻段 Attack 最低 8 ms、Release 最低 20 ms。
- Low-Mid Attack 最低 3 ms。
- High-Mid / High Attack 最低 0.5 ms。
- 所有頻段 Release 最低 5 ms。
- Gain 必須使用固定 5 ms 二次平滑以抑制 Click / zipper / transient tearing。
- 修改 OTT 後必須確認 build、50 次 DSP stress test 與相關 regression test。

## 發版前檢查

修改後至少確認：

- Native C++ 與 Web Preview 的核心演算法一致。
- Web Worklet JavaScript 可以正常解析。
+ 所有本次 GitHub 修改都使用對應版本號。
- Web Preview 不要求加入或更新日期／時間碼。
- 變更未重新引入 V1/V2/V3 舊版切換頁。

## Native VST3 / Web 雙版本同步（最高優先、強制）

凡是任何會改變 DSP 行為的改版，都必須同時修改 Native VST3 與 Web Preview，兩邊不得再分開演進。

強制同步範圍包含但不限於：
- 演算法、公式、waveshaping、harmonic injection。
- Attack / Release、Envelope、Detector、Gain smoothing。
- Crossover、頻段分割、Phase compensation、Latency。
- Threshold、Ratio、Depth、Mix、Gain、Output、Dry/Wet。
- Processing order、state machine、persistent state、reset 行為。
- Bypass、Solo、Stereo / Dual-Mono 行為，只要會影響聲音結果也必須同步。

規則：
1. Native 端的 Source/DSP/* 為正式 VST3 DSP 實作。
2. Web 端 docs/index.html 的 AudioWorklet DSP 必須同步實作同一版核心邏輯，不得使用簡化版、舊版或臨時替代演算法冒充同步。
3. 每次 DSP 改版至少要同時檢查兩邊的公式、參數範圍、state、processing order 與 bypass 行為。
4. 只有 UI 文字或排版可以單獨改；只要可能改變聲音結果，就視為 DSP 改版，必須 Native + Web 一起改。
5. 未完成其中一端時，不得宣稱該版本已完成、已同步或可發版。
6. GitHub Commit 使用對應版本號；若同一改版涉及多個檔案，可分 Commit，但不得讓其中一端長期停留在另一端的舊演算法。
7. CI / regression 必須至少確認 Native VST3 DSP 可建置，且 Web Worklet 可正常解析；兩端核心規則應以同一組測試基準比對。

### 同步驗收
發版前必須確認：
- Native VST3 與 Web Preview 核心 DSP 使用相同公式。
- 兩端的頻段 crossover / phase / detector / smoothing / gain 結構一致。
- 四頻段與左右聲道 state 定義一致。
- 不得再出現「Native 已更新，但 Web 還在跑舊演算法」的情況。

## CI/CD
- CI 的 Git checkout 必須保留完整 history（`fetch-depth: 0`），因為 Plugin/Web Preview 同步檢查需要比較 push 前後 commit。
