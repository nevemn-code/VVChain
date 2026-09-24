# Source/DSP 檔案總覽

- `ChainDSP.cpp`：完整 native 音訊運算；HP / CORNER 不參與聲音；UDMBC / TAPE COLOR 共用 Shared X-Over；每頻段提供 TT/SS Analog Color。
- `ChainDSP.h`：DSP 參數、四頻段色染模式、動態偵測器與 De-Esser 狀態。
