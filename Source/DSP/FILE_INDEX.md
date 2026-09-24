# Source/DSP 檔案總覽

- `ChainDSP.cpp`：完整 native 音訊運算；EQ、四段 Analog／UDMBC／TAPE COLOR、De-Esser、Mix／Solo／Limiter／Bypass／Delta。Analog、UDMBC、TAPE COLOR 共用 X1／X2／X3；每段各有 TT/SS Color。
- `VVChain_AnalogADAA_v2.h`：每段每聲道 Analog 一階 ADAA transfer 與 state。
- `ChainDSP.h`：DSP 參數、四頻段色染模式、動態偵測器與 De-Esser 狀態。
