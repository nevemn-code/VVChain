# VVChain 檔案總覽

## 根目錄
- `README.md`：專案功能、Signal Flow、Web Preview、Build 與驗證說明。
- `PROJECT_RULES.md`：指向 `.github/VVCHAIN_RULES.md` 的單一規則入口。
- `CMakeLists.txt`：JUCE 9.0.2、VST3、Standalone、可選 AAX 的 CMake 建置設定。
- `.gitignore`：Git 忽略規則。
- `LICENSE`：專案授權。

## 資料夾
- `Source/`：Native VST3 / Standalone 程式。
- `Tests/`：Reference、Web smoke、signal-integrity、Type-A ADAA 與 headless C++ null regression。
- `docs/`：GitHub Pages 預覽、架構與測試文件。
- `.github/`：GitHub Actions 與相關設定。
