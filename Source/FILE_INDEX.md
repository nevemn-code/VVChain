# Source 檔案總覽

- `PluginEditor.cpp`：Native UI、EQ graph、Shared X-Over、TT/SS 撥桿、所有旋鈕與 BYPASS LED、ADV 視窗。
- `PluginEditor.h`：Native UI 類別、knob / attachment / LED / TT-SS 控制結構。
- `SettingsPanel.cpp` / `SettingsPanel.h`：SETTINGS vector gear、UI-only overlay、內部捲動與 reserved/disabled section 架構。
- `PluginProcessor.cpp`：AudioProcessor、APVTS、processBlock、PDC、DSP 串接。
- `PluginProcessor.h`：AudioProcessor 宣告與 DSP 成員。
- `DSP/`：EQ / UDMBC / TAPE COLOR / De-Esser 的 native DSP。
