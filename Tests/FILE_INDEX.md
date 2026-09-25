# Tests 檔案總覽

- project_static_audit.py：版本、Native/Web 同步、routing 與死碼靜態稽核；Fast Gate 連跑 10 次。
- dynamic_eq_ui.py：Native/Web UI、Dynamic EQ、Q-wheel、bypass 與互動 regression。
- web_smoke.py：Web Preview / AudioWorklet syntax、UI 結構、DSP source contracts 與版本/cache parity。
- analyzer_matrix.py：主 Spectrum / DELTA Analyzer source contract。
- analog_matrix.py：Analog ADAA 500-case no-shrink / symmetry / X2 isolation regression。
- transient_matrix.py：Transient 5000-case control / stereo-link / bounded-gain regression。
- reference_stress.py：Analog、TAPE COLOR、Q 與 envelope coefficient reference stress。
