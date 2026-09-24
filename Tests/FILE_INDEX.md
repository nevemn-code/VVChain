# Tests 檔案總覽

- `project_static_audit.py`：Fast Gate whole-project 靜態稽核；CI 連跑 10 次。
- `dynamic_eq_ui.py`：Native/Web UI、Dynamic EQ、Q-wheel、bypass 與互動 source regression。
- `web_smoke.py`：Web Preview / AudioWorklet syntax、UI 結構與版本/cache parity smoke regression。
- `analog_matrix.py`：手動 Full Validation 的 Analog 500-case no-shrink / symmetry / X2 isolation regression。
- `reference_stress.py`：手動 Full Validation 的真實 DSP 數學 stress；驗證 Analog、TAPE-A、Q 與 envelope coefficient，不依賴 SciPy。
