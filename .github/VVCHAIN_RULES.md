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
4. **每一次 GitHub 更新，只要有任何程式碼、DSP、UI、互動、測試或 CI/CD 規則修改，都必須建立新的 PATCH 版本號；不得用同一版本號覆蓋不同次更新。**
5. 同一次原子提交內可以包含多個互相關聯的檔案，但只要再次提交新的修改，就必須再遞增版本號。
6. Web Preview 不需要使用日期／時間碼；版本號本身就是唯一的修改識別。
7. GitHub 自己產生的建立時間、Push 時間、Workflow 時間屬於 GitHub 平台資料，不納入 VVChain 版本規則。
8. Native VST3、Web Preview、Regression Tests 與對外可見的 CI/CD artifact，必須使用同一個目前版本號。

### 版本驗收

每次改版確認：
- Commit 有版本號。
- 需要對外識別的 Web Preview / Release 有對應版本號。
- 不再使用舊的時間碼規則。
- Native VST3 與 Web Preview 仍遵守雙版本同步規則。

## ANALOG 規則

ANALOG COLOR 正式基準自 **v1.0.16** 起為 unity-normalized smooth algebraic saturation + hard no-shrink guard。

- 0% COLOR 必須 exact dry / Delta 靜音。
- Native VST3 與 Web AudioWorklet 必須使用相同公式：奇對稱、無濾波 state、零額外相位旋轉。
- 核心 shaping 使用 `x / (1 + alpha*x^2)^(1/4)` 類型平滑曲線，並以 `|x|=1` normalization 避免 COLOR 增加時整體萎縮。
- shaping domain 限制在 -1..+1；超出範圍不得因 ANALOG 額外衰減。
- hard no-shrink guard 必須保證 COLOR 增加時，shaping domain 內每個 sample 的絕對值不得低於未染色值；0% 仍須 exact dry。
- 每個頻段仍保留獨立 COLOR、TT/SS、BYPASS、X2。
- **X2 仍只能把該頻段由 ANALOG COLOR 產生的 delta ×2；不得乘到 EQ、OTT、TAPE-A、DE-ESSER、MIX、OUT 或其他頻段。**
- 不再建立或保留 V1 / V2 / V3 Analog 選擇頁、切換頁或版本導覽。
- 修改 ANALOG 後必須完成 500-case regression matrix，並檢查 0% transparency、TT/SS 差異、odd symmetry、X2 delta isolation、finite output 與 no-shrink 邊界。

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
- 變更未重新引入 V1/V2/V3 舊版切換頁、舊版 HTML 或舊版選擇器。

## 每次更新版本號（最高優先、強制）

- **每一個新的 GitHub 修改批次都必須升 PATCH 版本。** 例如 v1.0.9 完成後下一次任何修改即為 v1.0.10，再下一次為 v1.0.11。
- 不得因「只是修 bug／只是補測試／只是修 Web」而沿用上一個版本號。
- 一次 GitHub 更新若包含 Native + Web + Tests，三者仍屬同一個新版本；後續再改任何一個檔案，就必須再升一版。

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

## 壓力測試與部署檢查

### Fast Deploy 規則（v1.0.21 起，最高優先）
- 一般 PR / main push 的預設驗證路徑必須以 **3 分鐘內完成工作執行** 為目標。
- Fast Gate 只保留會直接阻止錯版上線的必要項目：Native/Web 同步規則、版本規則、Web/Worklet JavaScript syntax、Web smoke、UI/互動 regression。
- 一般 PR **不得**再安裝整套 Linux audio/X11 開發套件，也不得每次重新跑 Linux VST3 全編譯、numpy/scipy 安裝、500-case ANALOG matrix 或 DSP stress。
- 完整 Linux Native build、500-case ANALOG matrix、5 次 DSP stress 移至 `workflow_dispatch -> full_validation=true`，需要深度驗證時才執行。
- Windows VST3 正式 artifact 只在 **main push / 手動 workflow** 建置；PR 階段不重複做 Windows Release build。
- Windows VST3 必須使用可恢復的 incremental build cache，避免每次從零編譯 JUCE。
- GitHub Pages 必須獨立於重型 Native CI，使用 docs-only sparse checkout + 最少必要 syntax/structure 驗證，不能等待 VST3 build 才部署。
- Pages workflow 設定 `timeout-minutes: 3`；Fast Gate 也設定 `timeout-minutes: 3`。若超時視為流程設計需要再優化，而不是把 timeout 往上放寬。
- GitHub hosted runner 的「排隊等待時間」不受 repository workflow 控制，因此 3 分鐘目標指 workflow 實際開始執行後；若要保證牆鐘時間，需改用常駐 self-hosted runner。

### 深度驗證
- GitHub 完整 DSP stress：**5 次**，只在 full validation 執行。
- ANALOG 修改後仍必須完成 500-case regression matrix，但不放在每次一般部署關卡。
- GPT 在提交前後的自我驗證仍維持 **10 + 10 次**；此規則不要求把 20 次都搬進 GitHub hosted runner。
- Native VST3 與 Web Preview 每次功能更新仍必須同步；Fast Deploy 只改驗證時機，不降低同步要求。

## CI/CD
- Fast Gate 為一般 PR 的必要 gate。
- main push 後，Web Pages 與 Windows VST3 應平行執行，互不等待。
- 只有需要比較完整 commit 範圍的 sync gate 使用 `fetch-depth: 0`；Pages / Windows artifact 使用淺層 checkout。
- CI verification branch: workflow changes must validated by an actual PR run before merge.
