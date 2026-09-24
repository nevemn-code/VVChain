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
- 每個頻段的 ANALOG COLOR 完全獨立；使用者範圍 0–60%，0% = exact dry；X2 只把目前 ANALOG delta 放大為 ×2。
- ANALOG COLOR 上方有 TT / SS 撥桿：TT = Tube Saturation；SS = Solid-State Saturation。
- 各 BAND 的 OTT / TAPE-A BYPASS 小燈固定位於對應旋鈕右上方；亮 = 啟用，暗 = BYPASS。
- DE-ESSER 框內四顆旋鈕垂直排列：DE-ESS FREQ / MAXIMUM REDUCTION / MIX / OUT。
- MAXIMUM REDUCTION 範圍 0–8 dB，預設 0 dB；DE-ESSER 預設不改變聲音。
- Master BYPASS 位於右上角，為全鏈旁通；啟用時 UI 灰階化。
- +ADV 開啟後，點視窗外即可關閉。

## DSP
- Native De-Esser 為 sample-domain split-band 處理；本身不再使用舊 8192-sample FFT/block PDC。
- HP / CORNER 不參與聲音計算。
- ANALOG COLOR 自 v1.0.16 起使用 unity-normalized smooth algebraic saturation + hard no-shrink guard：0% exact dry；奇對稱、無額外濾波相位；|x|=1 維持 unity；COLOR 增加不得讓 shaping domain 內 sample 絕對值縮小；X2 仍只放大該段產生的 ANALOG delta，v1.0.18 起為 ×2。
- Master BYPASS 保持固定 PDC，完全旁通時輸出延遲乾聲。
- AAX 目標受 VVCHAIN_ENABLE_AAX 控制，需合法 AAX SDK / 開發環境。

## Web Preview
- LOAD AUDIO：選取音檔後解碼，再 PLAY / STOP / RESET。
- Master BYPASS 會切至原始輸入並將整個介面灰階化。
- DE-ESSER 強度為 0 dB 時直接 bypass；大於 0 dB 時啟用 sample-domain split-band reduction。
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

### v1.0.4｜TAPE-A 啟動瞬間爆音修正
- TAPE-A 改為無狀態正規化 tanh 核心：不再用 Attack / Release / envelope state 決定增益，避免第一顆聲音因狀態初始化而突然放大。
- Drive 在參數區塊預先計算；以 tanh(drive) 作為 Makeup 分母，讓 |input|=1 的基準點維持 |output|=1。
- Native JUCE 與 GitHub Pages AudioWorklet 同步採同一套 4-band、stateless、normalized transfer；原有 Attack / Release 參數保留作為 preset/UI 相容，不再參與 TAPE-A 增益核心。
- 同時修正 TAPE-A 四段 crossover 重建方式為 LP1 / (LP2-LP1) / (LP3-LP2) / HP3，避免各頻段重疊累加造成額外電平。
- 此正規化保證的是 |input|≤1 的 0 dBFS 基準；內部超過 1.0 的 peak 仍由後級固定延遲 True-Peak Limiter 處理。
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
- ANALOG 正式基準自 v1.0.16 起為 unity-normalized smooth algebraic saturation + hard no-shrink guard；TT/SS/X2 各段獨立，不得重新引入 V1/V2/V3 選擇頁。

## Validation
- Tests/reference_stress.py：僅在手動 Full Validation 使用的 DSP stress helper；一般 PR 不執行。
- Tests/web_smoke.py：Web Preview JavaScript 語法、UI 結構、音檔載入 / 播放、BYPASS、DE-ESSER、MIX / OUT regression。

> Regression tests are not a substitute for final DAW pluginval or AAX certification.
  
### Analog Color / TT / SS
Analog Color uses the v1.0.16 unity-normalized smooth algebraic transfer with a hard no-shrink guard. Zero amount is exact dry, |x|=1 is normalized to unity, TT/SS use separate saturation depths, and The user control is capped at 60%, and X2 multiplies only the generated Analog Color delta by 2.

PSP's published ClassicQ documentation describes SIM as Class-A plus transformer simulation, with the Class-A stage before output level and the transformer followed by SAT; PSP does not publish the proprietary transfer curve. VVChain therefore uses that documented topology as a design reference rather than claiming a code-level clone. PSP describes its analog EQ/preamp coloration as gentle/subtle, and McQ describes SAT as a smooth overdrive stage.


## 開發同步規則（重要）
- **任何功能、UI、操作邏輯、參數、DSP 或互動修改，都必須同步檢查 JUCE Plugin 版與 GitHub Pages Web Preview。**
- JUCE 實作主要位於 `Source/`；GitHub Pages 實際執行版本位於 `docs/index.html`。
- **不能只修改 `Source/PluginEditor.cpp` / `ChainDSP.cpp` 就視為完成。** 若該功能在 Web Preview 存在，必須同步修改 `docs/index.html` 對應的 JavaScript / UI / DSP 模擬邏輯。
- 每次改版完成後，必須做「Plugin ↔ Web Preview」雙版本功能對照，至少確認參數範圍、拖曳方向、滑鼠事件、數值計算、UI 顯示與預設值一致。
- GitHub Pages 會從 `main` 的 `docs/` 部署；因此 Web Preview 的修正也必須直接提交到 `main`，並確認 Pages deployment 已觸發。
- **除非明確說明某功能只存在於其中一個版本，否則一律以雙版本同步為完成條件。**

## 版本規則

VVChain 只使用版本號標示修改版本，不再在 UI、Web Preview、測試或原始碼中寫入修改日期／時間戳。每次功能修改須同步更新 Native VST3、GitHub Pages Web Preview 與對應回歸測試的版本號。

## v1.0.36

- 依使用者提供的節點浮動框參考圖，改成 112px 的三行讀值：`EQ / DYN EQ + 頻率`、`GAIN`、`Q`。
- 頻率和增益顯示兩位小數，Q 顯示三位小數；Native 與 Web 使用同一種排列。

## v1.0.35

- 上方 EQ 與動態 EQ 浮動框在拖曳期間固定顯示被抓取的節點，放開後才重新判斷游標位置。
- Native 與網頁預覽同步擴充兩行浮動框寬度，完整顯示 GAIN、FREQ 與 Q；縮小靜態 EQ 的滑鼠命中範圍，避免遮住鄰近動態節點。
- 保持 EQ 與動態 EQ 各自獨立數值來源，浮動框不顯示其他參數。

## v1.0.34

- 上方浮動數值框命中規則改為確定式：滑鼠碰到 Static EQ 點時永遠顯示 `EQ GAIN`，不再被 Dynamic Live/Target 搶走。
- 只有滑鼠明確碰到 Dynamic Target 或右側 DYNAMICS 箭頭時，才顯示 `DYN EQ GAIN`。
- Dynamic Live 白點改為純視覺顯示，不再取得浮動數值框控制權。
- Web 的 DYNAMICS 箭頭按下瞬間也改用正式兩行 `DYN EQ GAIN / FREQ Q` formatter，不再暫時顯示 `DYN n xx%`。
- 新增 50-case Static EQ 與 Dynamic target 重疊/接近時的命中優先測試。

## v1.0.31

- 修正 Dynamic EQ regression 仍硬寫舊 `?v=1.0.18` 的問題；版本/cache 真正一致性改由通用 parity audit 驗證，不再每版維護舊常數。
- 本版不改 DSP 聲音公式；延續 v1.0.30 的專案清理、真實 stress test 與 Fast Deploy 優化。

## v1.0.30

- 第二輪專案除錯：Native EQ / DYN 不再維護舊 `graphHintBand / graphHintAutoHideAt` 狀態；舊 graph hint 只保留給 XOVER 拖曳，EQ / DYN 一律走兩行浮動框。
- `reference_stress.py` 移除永遠會 PASS 的 `tone - tone` 假 null test，改成 Analog odd/no-shrink/dry、TAPE-A unity、Q 連續性與 envelope coefficient 的真實數學檢查。
- Full Validation 移除 SciPy 依賴，只安裝 NumPy，縮短重型驗證準備時間。
- Fast Gate 新增 whole-project static audit，連跑 10 次檢查版本同步、Worklet cache、死碼、規則、CI cache 與部署設定。
- Analog 核心舊函式名稱 `processChebyshevAnalog` 改為 `processAnalogColor`；Web 標籤同步移除 CHEBYSHEV，DSP 公式完全不變。

## v1.0.29

- 專案第一輪除錯／清理：移除未使用 Analog scratch buffers、舊 Gyraf prototype、永遠無法執行的舊橫向 graph hint renderer 與假的 CMake echo test。
- 修正 Web 顯示版本已更新、但 AudioWorklet cache query 仍停在 v1.0.18 的問題；現在 CMake / Web / Worklet cache 版本會互相驗證。
- `PROJECT_RULES.md` 改為只指向 `.github/VVCHAIN_RULES.md`，避免兩套規則互相衝突。
- main push 不再重跑 PR Fast Gate；Pages 與 Windows VST3 獨立執行。
- Windows CI 只建 `VVChain_VST3` target、關閉 runner 本機 plugin copy，並改用穩定 incremental cache key，避免每次重新建立大型 cache。
- 更新 Architecture / Test Plan / Test Report / Third-Party notes，使文件與目前 DSP 架構一致。

## v1.0.28

- 上方 EQ 點的 Q 滾輪靈敏度提高為 v1.0.24 的 **3 倍**。
- 一般 EQ 滾輪與右鍵 SOLO + 滾輪仍共用同一個連續 exponential Q 公式，不做離散段落或步進。
- Native / Web：一般速度由 0.025 → 0.075；Shift fine 由 0.0025 → 0.0075。
- 新增 50-step 無段式連續性測試，確認每一步 Q 都是唯一且單調變化。

## v1.0.24

- 上方浮動數值框固定 112 px、固定兩行，不允許展開成橫向長條。
- 靜態 EQ 點／EQ 拖曳／Q 滾輪／右鍵 SOLO Q：第一行固定顯示 `EQ  GAIN …`。
- Dynamic Target／Live／DYNAMICS 箭頭／Dynamic 點 Q 滾輪：第一行固定顯示 `DYN EQ  GAIN …`。
- 第二行永遠只顯示 `FREQ …  Q …`；禁止 TARGET / OFFSET / DYN % / AUTO THR 等其他欄位進入浮動框。
- 新增 50-case readout identity / format regression。

## v1.0.23

- CI/CD 改成 Fast Deploy 預設路徑：一般 PR 只跑必要同步、版本、JS syntax、Web smoke、UI/互動 regression。
- 一般 PR 不再安裝 Linux audio/X11 開發套件、不再每次完整 Linux VST3 build、不再每次安裝 numpy/scipy。
- 500-case ANALOG matrix 與 5 次 DSP stress 移至手動 Full Validation。
- Windows VST3 Release 改為 main push / 手動 workflow 才建置，並加入 incremental build cache。
- GitHub Pages 改為 docs-only sparse checkout，與 Windows/Native CI 平行，Fast Gate 與 Pages 都以 3 分鐘執行時間為上限。
- GitHub hosted runner 排隊不受 repo 控制；若要保證從 push 到完成的牆鐘時間低於 3 分鐘，需要 self-hosted runner。

## v1.0.18

- 上方 EQ 點的一般滾輪與右鍵 SOLO 滾輪統一回到慢速、連續 Q 調整；Web 以每標準滾輪單位約 2.5% 比例變化，避免直接撞 0.1 / 18 上下限。
- ANALOG COLOR 使用者範圍由 0–100% 改為 0–60%；DSP 仍以百分比 /100 轉成 amount，因此 60% 對應原演算法 0.60 強度。
- ANALOG X2 改成真正將目前產生的 Analog delta ×2；不改原始乾聲。
- PEAK ↔ ONSETS 寬度 40 px → 60 px；拖曳改為相對式慢速 0.3×，並加入慢速滾輪控制。
- OTT %、TAPE-A +、DE-ESSER 的局部 LED BYPASS 與 0 值灰階連動；右上五個模組 BYPASS 也同步灰階其對應元件。
- 下區塊最右側主 BYPASS 文字改為置中在圓形主 BYPASS 按鈕正上方。
- 上方浮動值框固定兩行：EQ 或 DYN EQ 的 GAIN；第二行只顯示 FREQ + Q，依實際 hover 目標切換。
- 右鍵 SOLO EQ 點時，SOLO 中心保持原彩色，往左右頻率距離增加時線性淡入灰階。
- Native VST3 / Web Preview / AudioWorklet / Regression / Windows artifact 同步升至 v1.0.18。

## v1.0.17

- 修正 GitHub Pages 部署驗證仍硬檢查舊 `ANALOG #443` 的問題。
- Pages 現改為驗證目前 `ANALOG COLOR` 與 `unity-normalized smooth saturation` 標記，不再因舊基準字串阻擋 Web Preview。
- 本版不改 v1.0.16 已完成的 EQ / Dynamic EQ / ANALOG DSP 行為，只同步部署規則、Native/Web 顯示版本與 CI artifact 至 v1.0.17。

## v1.0.16

- 右鍵 + 滾輪與一般 EQ 滾輪共用同一個 Q 計算函式，方向與速度不再可能分岔。
- PEAK / ONSETS 寬度固定 40 px，約為原長條的一半，精準置中於 DYNAMICS 旋鈕上方。
- EQ 圖形垂直軸改為可逆非線性 mastering scale：±3 dB 最慢，3–6 dB、6–12 dB、12–18 dB 逐級加速；拖曳點與滑鼠 Y 座標完全一致。
- DYNAMICS 上下箭頭由 +30 px 再右移至 +44 px，hit area 縮成 6×14 px；靜態 EQ 中心 9 px 範圍具有優先權，避免誤抓 Dynamic EQ。
- 上方浮動框固定兩行，只顯示 EQ/DYN EQ GAIN，以及 FREQ + Q。
- ANALOG 加入 hard no-shrink guard；在既有 unity normalization 上再保證 COLOR 增加不會把 sample 絕對值變小，0% exact dry，X2 delta 規則不變。
- Native / Web Preview / Web Worklet / Regression / CI artifact 統一升至 v1.0.16。

## v1.0.15

- 上方 EQ 右鍵滾輪的 Q 調整與一般 EQ 滾輪統一為完全相同方向與速度；SOLO / 右鍵拖曳其餘行為不變。
- PEAK / ONSETS 比例控制寬度縮小約一半，定位到 DYNAMICS 旋鈕正上方中線。
- 上方 EQ GAIN 改為 cursor-anchored 非累積式分段加速：±3 dB 最細、3–6 dB 次之、6–12 dB 再加速、12–18 dB 最快。
- Dynamic EQ 上下箭頭右移並縮小 hit area；0% Dynamics 時中心區優先給靜態 EQ，降低誤拉 Dynamic EQ。
- 上方數值提示只保留兩行：EQ 或 DYN EQ 的 GAIN，以及 FREQ + Q。
- ANALOG 改為 unity-normalized smooth algebraic saturation，避免 COLOR 越開整體越小；0% exact dry，X2 仍只放大 ANALOG delta。
- Native / Web Preview / Web Worklet / Regression / CI artifact 統一升至 v1.0.15。

## v1.0.14

- Dynamic EQ 的 PEAK / ONSETS 改為 0–100% 連續混合，預設 50% / 50%；Native 與 Web DSP 同步以比例混合 onset detector。
- 上方 EQ 右鍵按住即 SOLO 該 EQ 頻率；右鍵拖曳同步調整 FREQ / GAIN，右鍵滾輪調整 Q，放開右鍵解除頻率 SOLO。
- PEAK / ONSETS 控制改為較短、寬版 TT / SS 類型比例條，比例填色使用藍色。
- Native / Web Preview / Web Worklet / Regression / CI artifact 統一升至 v1.0.14。

## v1.0.13

- 修正 EQ 上方曲線的頻率響應計算：Native / Web Graph 現在直接依目前 TPT Bell 拓撲計算，不再使用錯誤的舊複數響應公式。
- 修正正增益曲線反向、S 型與單一 Band 響應形狀異常；EQ 聲音處理核心保持 v1.0.12 不變。
- Native 與 Web Graph 使用相同的 TPT Bell state-space transfer evaluation。

## v1.0.12

- 修正 Web AudioWorklet Dynamic EQ 的 TPT Bell state 初始化缺漏：`ic1/ic2` 現在與 Native TPTBell 一樣明確初始化為 0。
- 新增防禦性 finite state 初始化，避免舊版 Web state 造成 NaN 進而被輸出安全閘切成全 0。
- Native VST3、Web Preview、Web Worklet、Regression Tests、CI artifact 統一升至 v1.0.12。

## v1.0.11

- Web Preview 新增真正的音訊檔案拖放載入：拖入頁面即可解碼並顯示檔名，阻止瀏覽器直接開啟音檔。
- 拖放載入與 LOAD AUDIO 按鈕共用同一個解碼流程。
- Native VST3、Web Worklet、Web Preview、Regression Tests 與 CI artifact 統一升至 v1.0.11。

## v1.0.10

- 本次更新正式升版；之後每一次新的 GitHub 修改批次都必須遞增 PATCH 版本，不再沿用上一版號。
- Native VST3、Web AudioWorklet、Web EQ response graph、Regression Tests 與 CI artifact 統一使用 v1.0.10。
- Web Dynamic EQ response graph 與 Native / Web TPT Bell DSP 保持同一套 Q / Bell 拓撲，避免「聲音已更新但 Web 曲線仍是舊演算法」的不同步問題。
- GitHub CI 壓測規則為 5 次；GPT 提交前後自我驗證規則為 10 + 10 次。

## v1.0.9

- 4-band Dynamic/Parametric EQ core replaced with double-precision Cytomic/Simper TPT Bell topology.
- Bell damping uses the exact relation k = 1 / (Q * A); the previous empirical extra Q reduction is removed.
- EQ state remains double precision while audio I/O stays float; 0 dB is structurally bit-transparent.
- Native updates the TPT Bell coefficients every sample, removing the old 4-sample coefficient stepping during Dynamic EQ movement.
- Web AudioWorklet and EQ response graph are synchronized to the same Bell topology/Q mapping.
- Added deterministic 50-case TPT Bell frequency/phase regression plus 50-case per-sample modulation stability stress testing.
## v1.0.8

- 上方 EQ / DYNAMICS 改為絕對游標座標映射；X/Y 都直接跟隨滑鼠，不再用累積位移或抓取偏移造成越跑越遠。
- Graph 可用 GAIN 視覺範圍調整為 ±18 dB，與實際參數範圍一致。
- DE-ESS MODE 改為 I / II / III / IV 四段撥桿。
- DE-ESSER 右側 BYPASS 位置／尺寸重新對齊；MIX / OUT 下移。
- LOAD AUDIO 後的 0:00.000 固定緊接在右側，不再因版面擠壓跑到其他列。
- ANALOG COLOR 正式基準改為 Deploy VVChain Web Preview #443；移除 V1 / V2 / V3 舊頁面。
- ANALOG COLOR X2 僅將該段染色 delta 放大 1.6 倍。

- 上方 EQ／DYNAMICS 頻率拖曳改為直接依滑鼠座標反算，保留抓取偏移，不再使用會造成超前的跟隨倍率。
- DE-ESS MODE 改為四段離散旋鈕，10–14 點鐘方向顯示 I／II／III／IV。
- 上方浮動數值框縮小，只顯示 EQ 或 DYN EQ 的 GAIN、FREQ、Q。
- EQ FREQ 與 DE-ESS FREQ 拖曳靈敏度降低到接近 GAIN 手感；Web Preview 與 Native 同步。
- TAPE-A 最大染色上限固定為 Band 1=50%、Band 2=60%、Band 3=70%、Band 4=90%，演算法本體不改。
- 每段 ANALOG COLOR 新增 X2 開關；開啟後只將當前 COLOR 量乘以 1.6，並保留 100% 實際處理上限。
