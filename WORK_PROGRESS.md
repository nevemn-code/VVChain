# VVChain Work Progress

## Goal

Replace the rejected/approximate VVChain UI asset pipeline with a Master-locked PNG workflow, then migrate Native and Web to one shared set of Master-validated runtime assets without changing DSP, parameter IDs, automation, mouse behaviour, processing order or preset compatibility.

## Current version

v1.0.85

## Current checkpoint

**UI3 G01 in progress — full-panel RGBA derivative PASS locally; clean graph-bed remains blocked by strict Master-lock; production runtime unchanged**

## Master paths

Exact original Visual Master binaries:

- `assets/ui/master/VVChain_black_master.png`
- `assets/ui/master/VVChain_ivory_master.png`

Locked identities:

- Black: 1448×1086 RGBA, 3,012,513 bytes, SHA-256 `173d086d919d0a0b5c0c3100a060dc1929a918f9c1f8e4328424cb2e5e817b38`
- Ivory: 1448×1086 RGBA, 3,115,458 bytes, SHA-256 `9f99da776c8f081abd512bae269c79e47524c011cde190a56c2172ec9957effb`

## Final production state

- The approved Master PNGs are the only Visual Masters.
- The old Python/procedural UI renderer is retired.
- `assets/ui/runtime/APPROVED.lock` is present.
- Exactly 46 approved runtime PNGs are committed under `assets/ui/runtime/`.
- Every runtime PNG is locked by byte count, SHA-256, dimensions and 8-bit RGBA contract in `binary_manifest.json`.
- Runtime source archive identity is locked: `VVChain_UI_Runtime_Engineering_v1.0.74.zip`, 7,322,749 bytes, SHA-256 `1fca9eca20ccc1852310287fe41fbd59341aa018726c3ce764e934b2a7e6c297`.
- Six normal metal knob colours are active: Gold, Blue, Green, Red, Black and Silver.
- Platinum Master Gain uses the 1.5× runtime sprite.
- Knob state engineering used X4 sampling and 64 states; the amber outer LED ring remains fixed.
- Native and Web use the same approved root runtime source.
- Pages deploys a byte-for-byte staged copy of the approved root runtime.
- Native production CMake requires the approved Master runtime and no longer links the rejected Candidate bundle.
- Web production runtime is Master-only; the rejected Candidate CSS/path is removed.
- Rejected legacy `docs/assets/ui/png/` Candidate files are completely deleted: 0 remain.
- UI labels now identify the dark skin as BLACK / BLACK GOLD instead of the rejected STUDIO TEAL wording.

## v1.0.84 reference-layout checkpoint

- UI-only change. DSP, parameter IDs, automation, processing order and preset compatibility are unchanged.
- Native and Web frame geometry now use the supplied third-reference 1448×1086 basis.
- EQ graph and five-column lower section were repositioned/resized to the reference proportions.
- Band control lanes now follow the reference vertical rhythm: EQ → Dynamic EQ → UDMBC → Analog/Tape/Transient.
- Normal controls select the already-approved Silver runtime knob sprite; OUTPUT_LEVEL retains the existing 1.5× Platinum Master sprite.
- Existing Black/Ivory panel, button, LED, toggle, slider and knob PNG binaries are reused byte-for-byte.
- No PNG was edited, recoloured, regenerated, replaced or added in this checkpoint.
- `APPROVED.lock`, `binary_manifest.json` and all previously locked runtime hashes remain unchanged.

## v1.0.85 upper-section checkpoint

- Upper section only. Lower band-strip layout is intentionally untouched in this checkpoint.
- Native/Web header controls are redistributed and enlarged for legibility while retaining the same functions.
- The EQ/Dynamic EQ viewport is now one continuous 20 Hz–20 kHz graph.
- The obsolete split graph, duplicated grid and duplicated frequency labels baked into the older full-panel background are covered only at runtime inside the live graph viewport.
- Analyzer, EQ nodes, Dynamic EQ, X1/X2/X3, hover readout, SOLO spotlight and graph gestures remain on the same current logic.
- No Master/runtime PNG was edited, replaced, recoloured, regenerated or deleted.
- APPROVED.lock, runtime hashes and binary_manifest.json remain unchanged.

## Validation evidence

- Issue #45 binary ingest: PASS — exact archive SHA/bytes verified and 46/46 PNGs matched the locked per-file manifest before `APPROVED.lock` was created.
- PR validation, Fast CI/CD #1369 / run 36226292789: PASS.
  - Fast Gate: PASS.
  - JavaScript / Web smoke ×10: PASS.
  - Whole-project static audit ×10: PASS.
  - UI Master Lock gate: PASS.
  - UI / interaction regression ×10: PASS.
  - Analyzer contribution matrix ×10: PASS.
  - Full Native / DSP Validation: PASS.
  - Windows VST3 Release build/package/upload: PASS.
- PR #44 merged to `main` as `a83f665332650608b4c5ec3774ee12be6aad51f7`.
- Main Pages #544 / run 36226682246: PASS.
  - Master runtime APPROVED + binary hashes: PASS.
  - Pages approved Master runtime contract: PASS.
  - Web version/cache parity: PASS.
  - GitHub Pages deployment: PASS.
  - Deployment URL: `https://nevemn-code.github.io/VVChain/`.
- Main Fast CI/CD #1370 / run 36226682254: PASS.
  - Fast Gate: PASS.
  - Windows VST3 Release: PASS.
  - Main artifact: `VVChain-v1.0.83-Windows-VST3`.
- Final main repository check:
  - approved runtime PNGs: 46
  - rejected Candidate PNGs under `docs/assets/ui/png/`: 0
  - exact Master PNGs: 2
  - `APPROVED.lock`: present

## Safety / compatibility constraint

Future UI changes remain governed by `UI_MASTER_LOCK.md`. Do not regenerate the approved UI from descriptions, SVG, CSS, Canvas, Python/procedural renderers or approximate substitutes. Native and Web must continue to share the same approved runtime source.

## UI3 approved-visual checkpoint

- Branch remains `ui/ui3-hardware-gold-candidate`.
- The user explicitly approved the latest Hardware Gold / Amber UI visual as the third UI Visual Master source.
- Latest approved source identity (supersedes all earlier UI3 candidate hashes):
  - intended repository path: `assets/ui/master/VVChain_hardware_gold_master.png`
  - dimensions: 1448×1086
  - source mode: RGB PNG
  - bytes: 1,959,313
  - SHA-256: `ae8401ad7e3aa477ebcceed2a6ab45a19275500fd9e24a98a87d372c31f0382b`
- Maximum-fidelity UI3 extraction BOM is locked in `assets/ui/master/UI3_ASSET_BOM_PENDING.json`: 38 planned PNGs in 10 checkpoint groups.
- Exact Master is now present on the UI3 branch and byte-identity verified: Git blob `daae37024c5af20e54ed4263d4bb6a3eb9f57af1`, 1,959,313 bytes, SHA-256 `ae8401ad7e3aa477ebcceed2a6ab45a19275500fd9e24a98a87d372c31f0382b`. Master ingest commit: `cf2cb74f7485f976c234a0aa279a07bb7239a1fd`.
- No production Master/runtime PNG has been changed and main remains on the v1.0.85 two-skin runtime.
- No DSP, APVTS parameter, automation, processing-order or audio change is permitted as part of UI3.
- Knob-animation invariant is now locked for UI3: every frame must use an identical canvas/anchor/centre; the knob body must remain pixel-stationary and only the pointer/state may change. Any frame-to-frame body translation is a FAIL.

## Remaining

**Two-phase plan active: Phase 1 = all non-knob UI assets; Phase 2 = rotary knobs from user-supplied GitHub/open-source code**

## G01 current evidence

### G01A — hardware_gold_full_panel.png
- Source: locked `assets/ui/master/VVChain_hardware_gold_master.png`
- Method: RGB→RGBA only; no resampling, redraw, recolour, crop or material change.
- Output dimensions: 1448×1086
- Output mode: RGBA
- Alpha extrema: 255..255
- RGB pixels vs Master: byte-identical
- Output bytes: 2,206,153
- SHA-256: `2f9419ddc6517b28e1c041c4a0f1d691889b6e3327492ad68a05b705de3802a9`
- Validation: **PASS locally**
- Runtime admission: **NOT YET** — binary is not committed to the branch yet.

### G01B — hardware_gold_graph_bed.png
- The locked Master graph contains baked live EQ response curves and numbered nodes 1–4.
- A clean graph bed cannot be obtained by crop/alpha conversion alone.
- Removing those baked live objects by generative fill, procedural redraw, CSS/Canvas recreation or AI inpainting would violate `UI_MASTER_LOCK.md`.
- Therefore G01B is currently **BLOCKED**, not PASS, and must not enter runtime.

## Next action

1. Preserve G01A as the verified full-panel derivative.
2. Resolve G01B only by a Master-compliant method: exact non-generative source pixels supplied/approved for a clean graph bed, or an explicit policy change authorizing destructive cleanup.
3. Do not start G02 and do not admit G01 to runtime until G01B passes and both binaries are committed.


## UI3 two-phase execution change

User changed the delivery plan:

### Phase 1 — ACTIVE
Generate and validate all UI3 assets except rotary knob sprites:
- G01 backgrounds
- G03 wide dark buttons
- G04 +ADV buttons
- G05 settings square
- G06 LED states
- G07 TT/SS metal toggle
- G08 screws / X-OVER slider hardware
- G09 1st/2nd indicator square
- G10 value plate

### Phase 2 — DEFERRED
- G02 rotary knobs only.
- Do not regenerate, approximate, recolour or procedurally build knob sprites.
- User will provide GitHub/open-source knob source/implementation later.
- Existing knob work is not to be treated as final UI3 knob source.

### Phase 1 rule
Candidate files remain outside production runtime until their own Master comparison / RGBA / alpha / dimension / state-geometry checks pass. Runtime Native/Web parity remains mandatory.
