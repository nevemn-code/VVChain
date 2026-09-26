# VVChain 檔案總覽

## 根目錄
- `README.md`：專案功能、Signal Flow、Web Preview、Build 與驗證說明。\n- `PROJECT_RULES.md`：指向 `.github/VVCHAIN_RULES.md` 的單一規則入口。
- `CMakeLists.txt`：JUCE 9.0.2、VST3、Standalone、可選 AAX 的 CMake 建置設定。
- `.gitignore`：Git 忽略規則。
- `LICENSE`：專案授權。

## 資料夾
- `Source/`：Native VST3 / Standalone 程式。
- `Tests/`：Reference 與 Web smoke regression。
- `docs/`：GitHub Pages 預覽、架構與測試文件。
- `.github/`：GitHub Actions 與相關設定。

## UI Master Lock / 長任務恢復
- `UI_MASTER_LOCK.md`：UI Master 唯一來源、驗收與禁止重新生成規則。
- `WORK_PROGRESS.md`：長任務 checkpoint、已完成/剩餘項目與下一步。
- `assets/ui/master/`：只存放使用者確認的原始 Master PNG；不得由 renderer 產生。

- `UI_ASSET_MAP.md`：已核准衍生素材的功能對應、旋鈕配色與 Runtime 合約。\n