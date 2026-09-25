# VVChain Test Plan（v1.0.56）

v1.0.55 已修正先前已知的 Web smoke Python 引號錯誤與 UI regression 過時固定版本／註解 guard。Fast Gate 現在在 PR、main push、手動執行三種情況都會跑；實際通過與否仍以本版 Actions 為準。

## Default Fast Gate

Runs on pull requests, main pushes and manual workflow dispatch, and is intentionally lightweight:

- Source ↔ Web Preview synchronization rule.
- Version-only rule.
- Web AudioWorklet JavaScript syntax.
- Web smoke regression.
- Dynamic EQ / UI / interaction regression.
- Current Web visible version ↔ Worklet cache version parity.

目前 `web_smoke.py` 第 82 行 Python 語法錯誤，PR gate 不能通過；其他 source guard 也有舊字面斷言。先修測試，確定它們真正執行到測試核心，再宣稱 Fast Gate 通過。

The Fast Gate must not install the full Linux audio/X11 toolchain or rebuild Native VST3.

## Main push

- GitHub Pages deploys independently from `docs/`.
- Windows builds only the `VVChain_VST3` target.
- Fast Gate is repeated on the main push.
- The Windows build reuses a stable incremental JUCE/MSVC build cache and does not copy the plugin into the runner's local plugin folder.

## Manual Full Validation

Manual `full_validation=true` is reserved for expensive checks:

- Linux VST3/DSP build.
- Analog 500-case matrix.
- DSP stress test.
- 檢查腳本不應在 500 cases 或 DSP 迴圈前因註解、換行或 include 所在檔案而結束。
- Additional host/pluginval checks when available.

## Current architecture checks

- Four Parametric / Dynamic EQ bands.
- Shared X1 / X2 / X3 crossover boundaries.
- Four independent UDMBC bands and bypass states.
- Four TAPE COLOR bands sharing the crossover ranges.
- Four independent Analog Color bands, 0–60 processing range, TT/SS, bypass and X2 delta ×2.
- De-Esser 6–18 kHz, 0–8 dB maximum reduction, four response modes.
- LF／HF Roll-Off 6／12／24／36／48／60／72 dB/oct，預設 12；逐格 OCT 控制須實測滑鼠滾輪。
- Compact two-line EQ / DYN EQ graph readout.
- Master bypass / dry path latency alignment.

## Host validation before public release

Still required when preparing a distributable release:

- pluginval
- target DAWs
- mono / stereo
- sample-rate and block-size changes
- automation and state recall
- bypass / dry-wet / Delta
- NaN / infinity / denormal behavior
- AAX SDK build/signing when AAX is enabled
- 超過 prepare block 容量的 host buffer，以及 Analog 0% 的全鏈 null／分頻重建，必須獨立量測。


## Settings overlay regression (v1.0.50)

- Confirm top-row order remains BYPASS / SOLO PRE / EQ / UDMBC / ANALOG / TAPE COLOR / DE-ESS / SETTINGS.
- Confirm SETTINGS is 28×28 px, vector-rendered, right-aligned, and opens a 330 px panel toward the left/bottom.
- Confirm second gear click, outside click and Escape close the overlay.
- Confirm panel overflow is internal and the plugin/page itself never gains a scrollbar.
- Confirm opening/closing the overlay does not write APVTS, send Web Worklet parameters, change DSP state, latency, or plugin dimensions.
- Confirm all not-yet-implemented settings are visibly RESERVED / DISABLED rather than interactive no-op controls.


## Theme regression (v1.0.52)

- New instance / page load must start in DARK.
- SETTINGS -> THEME toggles DARK <-> IVORY without closing/reopening the plugin.
- Theme switch must not call APVTS parameter writes or Web sendParams().
- Parameter values, DSP output, latency, bypass state, Solo/Delta state and control geometry must remain unchanged across a theme switch.
- Native and Web should preserve band/module accent colours while changing neutral surfaces/text to the ivory palette.


## Spectrum / Web runtime regression (v1.0.53)

- Web page load must complete without a runtime exception before `state` initialization; Theme default initialization must not call `drawEQ()` early.
- Analyzer default is ON in Native and Web.
- Native Analyzer uses a pre-DSP input tap, 2048-point Hann FFT and 220 display points; no FFT or allocation may run inside `processBlock()`.
- Analyzer OFF must stop Native analysis writes/FFT and disconnect the Web analyzer branch/timer.
- Web Analyzer branch must remain parallel to the source -> Worklet/fallback audio path and end in a zero-gain sink.
- Switching Analyzer ON/OFF or DARK/IVORY must not change APVTS values, DSP output, latency, bypass/solo/delta state, or control geometry.


## Analyzer smoothness regression (v1.0.54)

- Native and Web must both use 4096-point FFT analysis and 256 log-frequency display points.
- Frequency aggregation must use RMS energy over an approximately 1/12-octave window, with at least three bins at the bass end.
- The display must use frequency smoothing plus asymmetric temporal attack/release smoothing.
- Final rendering must use a cubic path rather than raw line-to-line FFT vertices.
- Analyzer ON/OFF must still leave APVTS, DSP output and reported latency unchanged.
- Windows VST3 build must verify Analyzer fields live in the editor instance, not in MetalLookAndFeel.


## Main + contribution analyzer regression (v1.0.55)

- Main analyzer: 4096 Hann, 50% overlap / 2048 hop, 256 log display points.
- Main display must use power averaging, seven-tap binomial smoothing, asymmetric attack/release, 4.5 dB/oct @ 1 kHz tilt and monotone cubic interpolation.
- Module contribution identity is fixed: ANALOG orange/gold, UDMBC cyan/blue, TYPE-A purple/pink.
- Contribution source is each module's local Post-Pre Delta, but upward drawing is allowed only where Post power > Pre power and Post is above the silence gate.
- Native must expose synchronized Analog/UDMBC/TYPE-A pre/post analysis streams without feeding those streams back to audio.
- Web Worklet must send contribution data only while analysis is enabled; hidden tabs must stop analyzer work.
- Analyzer OFF / editor closed must remove contribution FFT/FIFO work while leaving DSP output, latency and parameters unchanged.
- Visual growth is capped at 8 dB per module and 12 dB combined.
- Fast Gate runs Web smoke x10, whole-project static audit x10 and UI/interaction regression x10 on the main push.
