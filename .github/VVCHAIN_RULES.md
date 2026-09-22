# VVChain GitHub 開發規則

LAST MODIFIED 2026-09-22 21:28（Asia/Taipei / UTC+8）

## 全部 GitHub 動作時間碼（最高優先、強制）

凡是任何會寫入或修改 GitHub 的動作，都必須留下本次實際動作的台灣時間碼，不只網頁版。

適用範圍包含但不限於：

1. 修改 / 新增 / 刪除任何原始碼、Header、測試、Workflow、文件、設定檔。
2. GitHub Commit / Push。
3. Pull Request、Issue、Comment、Review 等 GitHub 內容寫入。
4. GitHub Pages / Web Preview。
5. 任何 CI/CD、測試規則或部署規則的修改。
6. 一次操作涉及多個檔案時，每次寫入行為仍必須使用實際時間碼。

統一格式：

`[YYYY-MM-DD HH:MM:SS TST]`

其中 TST = Taiwan Standard Time，固定使用 Asia/Taipei（UTC+8）。

### Commit 規則

所有 GitHub Commit message 必須以本次實際台灣時間碼開頭，例如：

`[2026-09-22 21:28:35 TST] DSP: update OTT smoothing`

不得只靠 GitHub 自動顯示的 commit 時間；時間碼必須明確寫入 commit message。

### PR / Issue / Comment / Review 規則

凡建立或修改上述內容，正文第一行必須放本次實際時間碼：

`TIMECODE: 2026-09-22 21:28:35 TST`

### Workflow / CI/CD 規則

凡修改 `.github/workflows/*`：

- Commit message 必須有時間碼。
- Workflow 本身若有可見版本 / 狀態紀錄，也必須保留時間碼。
- 不得只替 Web Preview 加時間碼，而忽略 CI、測試或其他 GitHub 動作。

## Web 頁面時間碼（強制）

凡是任何會修改 `docs/*.html` 的工作：

1. 必須同步更新頁面內的：
   `LAST MODIFIED YYYY-MM-DD HH:MM`
2. 時區固定使用台灣時間（Asia/Taipei / UTC+8）。
3. 時間碼必須是該次實際修改的時間，不得沿用舊時間。
4. 若一次修改多個網頁，所有被修改的網頁都必須更新時間碼。
5. 不得只修改 GitHub commit message 而不更新頁面內時間碼。
6. 不得移除時間碼。
7. 同時仍必須遵守「全部 GitHub 動作時間碼」規則；Web 頁面的時間碼不能取代 Commit 時間碼。

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
- 所有本次 GitHub 寫入行為都有時間碼。
- 被修改的 Web 頁面都有頁面內時間碼。
- 變更未重新引入 V1/V2/V3 舊版切換頁。
