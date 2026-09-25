# Source/DSP 檔案總覽

- `ChainDSP.cpp`：完整 Native 音訊運算；EQ／Dynamic EQ、四段 TRANSIENT、Analog 4× ADAA parallel-delta、stereo-linked UDMBC parallel-delta、Type-A/TAPE COLOR analytical ADAA、Mix／Solo／true-peak Limiter／Bypass／Delta，以及 large-block chunk safety。
- `VVChain_AnalogADAA_v2.h`：每段每聲道 Analog 一階 ADAA transfer 與 state。
- `ChainDSP.h`：DSP 參數、linked UDMBC state、Type-A ADAA state、LR4 crossover/cache、fixed PDC、neutral limiter dry path 與預配置 realtime buffers。
