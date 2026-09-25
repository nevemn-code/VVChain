# Tests 檔案總覽

- `project_static_audit.py`：版本、Native/Web 同步、routing、dead-code、CI policy 與規則靜態稽核。
- `dynamic_eq_ui.py`：Native/Web UI、Dynamic EQ、Q-wheel、bypass 與互動 regression。
- `web_smoke.py`：Web Preview / AudioWorklet syntax、LR4 routing、UI 結構、DSP source contracts 與版本/cache parity。
- `analyzer_matrix.py`：主 Spectrum / DELTA Analyzer source contract；module contribution analyzer 必須完全不存在。
- `analog_matrix.py`：Analog ADAA 500-case no-shrink / symmetry / X2 isolation regression。
- `transient_matrix.py`：Transient 5000-case control / stereo-link / bounded-gain regression。
- `typea_adaa_matrix.py`：Type-A analytical ADAA unity / finite / alias reduction。
- `signal_integrity_matrix.py`：UDMBC stereo-link、Native/Web LR4、coefficient lazy path、large-block、neutral limiter guards。
- `dsp_null_test.cpp`：真正執行 ChainDSP 的 neutral full-chain null、DELTA 及 70,000-sample large-block test。
- `reference_stress.py`：Analog、Type-A ADAA、Q 與 envelope coefficient reference stress。
