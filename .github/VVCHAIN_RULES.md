# VVChain GitHub 開發規則

## Web 頁面時間碼（強制）

凡是任何會修改 `docs/*.html` 的工作：

1. 必須同步更新頁面內的：
   `LAST MODIFIED YYYY-MM-DD HH:MM`
2. 時區固定使用台灣時間（Asia/Taipei / UTC+8）。
3. 時間碼必須是該次實際修改的時間，不得沿用舊時間。
4. 若一次修改多個網頁，所有被修改的網頁都必須更新時間碼。
5. 不得只修改 GitHub commit message 而不更新頁面內時間碼。
6. 不得移除時間碼。

## ANALOG 規則

目前正式主版本使用 V3 CHEBYSHEV。

- ANALOG 必須維持 4 個獨立頻段。
- 每個頻段有自己的 ANALOG COLOR。
- TT 與 SS 必須是各自獨立的演算法設定，不得平均、合併或共用。
- 每個頻段的 TT/SS 選擇必須只影響該頻段。
- 不得把四段 ANALOG 改成單一全頻 ANALOG。
- 修改 ANALOG 後，至少執行既有 500-case regression matrix。

## 發版前檢查

修改後至少確認：

- Native C++ 與 Web Preview 的核心演算法一致。
- Web Worklet JavaScript 可以正常解析。
- 網頁時間碼已更新。
- 變更未重新引入 V1/V2/V3 舊版切換頁。
