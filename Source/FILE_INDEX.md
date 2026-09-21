# Source 檔案總覽

- `PluginEditor.cpp`：Native UI、EQ graph、FFT、X-Over、所有旋鈕與 BYPASS LED。
- `PluginEditor.h`：Native UI 類別、knob / attachment / LED 結構。
- `PluginProcessor.cpp`：AudioProcessor、APVTS、processBlock、PDC、DSP / FFT 串接。
- `PluginProcessor.h`：AudioProcessor 宣告與 DSP / Spectrum Analyzer 成員。
- `SpectrumAnalyzer.cpp`：背景 FFT 與平滑資料產生。
- `SpectrumAnalyzer.h`：Spectrum Analyzer 介面。
- `DSP/`：EQ / OTT / TAPE-A / De-Esser 的 native DSP。
