# Source/DSP 檔案總覽

- ChainDSP.cpp：完整 Native 音訊運算；EQ／Dynamic EQ、四段 TRANSIENT、Analog parallel-delta、UDMBC parallel-delta、TAPE COLOR、Mix／Solo／Limiter／Bypass／Delta。Analog、UDMBC、TAPE COLOR 與 TRANSIENT 共用 X1／X2／X3 頻段定義。
- VVChain_AnalogADAA_v2.h：每段每聲道 Analog 一階 ADAA transfer 與 state。
- ChainDSP.h：DSP 參數、四頻段非線性／動態 state、crossover、固定 PDC 與 limiter state。
