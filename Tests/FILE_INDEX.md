# Tests 檔案總覽

目前測試報告記錄的 v1.0.48 基準中，五個腳本均因語法或已過期 source guard 提前失敗；下列是設計用途，不代表驗證已通過。詳見 `docs/TEST_REPORT.md`。

- `project_static_audit.py`：Fast Gate whole-project 靜態稽核；CI 連跑 10 次。
- `dynamic_eq_ui.py`：Native/Web UI、Dynamic EQ、Q-wheel、bypass 與互動 source regression。
- `web_smoke.py`：Web Preview / AudioWorklet syntax、UI 結構與版本/cache parity smoke regression。
- `analog_matrix.py`：手動 Full Validation 的 Analog 500-case no-shrink / symmetry / X2 isolation regression。
- `reference_stress.py`：手動 Full Validation 的真實 DSP 數學 stress；驗證 Analog、TAPE COLOR、Q 與 envelope coefficient，不依賴 SciPy。
