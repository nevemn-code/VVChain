# VVChain Work Progress

## v1.0.87 fixed-axis Ivory rotary checkpoint

- Re-registered all 56 frames of the previously approved high-resolution Ivory knob source to a single 176×176 cell centre and a common ring radius.
- Measured source frame centre span: 5.609 px horizontal / 3.626 px vertical; corrected candidate: 0.504 / 0.334 px. Radius span: 1.820 → 0.300 px.
- Frame 0 supplies fixed outer ticks and backdrop; the approved source supplies each rotating dial face. No new metal material or colour scheme was generated.
- The runtime pathname remains `assets/ui/runtime/knobs/knob_reference_hires_56.png`, shared by Native binary resources and Web CSS. Existing 8×7 / 56-frame selection and sound/parameter logic are unchanged.
- Runtime manifest, scoped staged manifest, approval lock, Web/Native version display and cache version updated together. This checkpoint is independent of the newer 154115 all-component Candidate; that archive is not accepted into runtime.
- Verification pending at this checkpoint: Master lock, Web smoke/static audit, UI interaction regression and Native build. Do not call the deployment complete until CI confirms.

## 154115 full-component candidate audit

- Component inventory and detailed acceptance results: `docs/UI_154115_ASSET_AUDIT.md`.
- 106 source-visible components produced as engineering candidates, each at original size and a fourfold resampled size; unchanged Master and enlarged panel also included (216 PNGs total).
- ZIP verified: `VVChain_154115_components_CANDIDATE.zip`, SHA-256 `533fc03ef5db2ac73a875f47e83bc62b5f98719b71711ab599872527037c770c`.
- Visible RGB source pixels, PNG decoding, RGBA/dimensions and ZIP integrity: PASS. Independent transparent edges, missing states, intrinsic 4× material detail and clean control-free backgrounds: FAIL / unavailable.
- No generated or candidate image is placed in production runtime. Native and Web remain at existing approved assets; product version remains v1.0.86.

## New visual source checkpoint — 154115 (2026-09-26)

- The user's newer, higher-resolution `image(20260926-154115).png` is the current full-panel visual reference for this request. Its exact bytes are preserved at `assets/ui/master/VVChain_20260926_154115_master.png`; see adjacent identity JSON.
- It supersedes OKK(3) for current visual acceptance; OKK(3) remains as historical intake, not the active visual source.
- Source identity: 1456×1080, RGBA, alpha 255 everywhere, SHA-256 `479bf03e2ff528ece86536d5d65f1aa77a54247e4cb1b25d2eff53969f3654b2`.
- The user explicitly permits reconstructing isolated PNGs so long as exact colours, material and geometry are preserved. This does not establish 100% fidelity from an opaque single frame.
- First high-resolution transparent knob generation compared visually against source: **FAIL** (ring thickness, highlight and pointer shape differ). It is not admitted to runtime.
- Runtime migration and version bump remain blocked until component groups and their non-default states pass source comparison. Preserve existing Native/Web behavior meanwhile.

## New request checkpoint — OKK(3) master intake (2026-09-26)

- Branch: `ui/okk3-master-checkpoint`; production `main` remains at v1.0.86.
- The exact uploaded `OKK(3).png` is preserved byte for byte at `assets/ui/master/OKK3_20260926.png` and identified in `assets/ui/master/OKK3_20260926.identity.json`.
- Scope: the user's new reference for panel appearance and arrangement. Earlier background and component PNGs are **Candidates** for this request until compared to this master; their prior approval does not prove fidelity to OKK(3).
- The source is 1265×938 RGBA, but its alpha is entirely opaque and its controls, lettering, bezel, shadows and background are flattened together.
- Acceptance: exact source identity PASS. New runtime backgrounds, isolated rotary states, buttons, LEDs, bypass states and transparent component assets: NOT YET ACCEPTED. Native/Web replacement: NOT STARTED. No claim of fourfold intrinsic resolution or exact interactive reconstruction.
- Blocker: the supplied single frame has no hidden background pixels beneath controls and no off/pressed/disabled states or rotating knob angles. Fourfold resampling only creates more pixels; it cannot recover the missing scene detail or states. Do not interpolate these and describe them as master-extracted high-detail assets.
- Next action: obtain a genuinely higher-resolution approved export with separable background/components and states, or explicit additional masters for them; then extract one group at a time, inspect against the master at 100%, 200%, 400%, validate RGBA/alpha/dimensions, and checkpoint each passing group before switching shared Native/Web runtime.

## Goal

Replace the rejected/approximate VVChain UI asset pipeline with a Master-locked PNG workflow, then migrate Native and Web to one shared set of Master-validated runtime assets without changing DSP, parameter IDs, automation, mouse behaviour, processing order or preset compatibility.

## Current version

v1.0.86

## Current checkpoint

**v1.0.86 — Exact band ordering + user-approved high-resolution knob runtime**

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

## v1.0.86 exact-order / high-resolution knob checkpoint

- Confirmed uploaded scoped masters:
  - `assets/ui/master/UI_LAYOUT_MASTER_EXACT_ORDER.png`
  - `assets/ui/master/KNOB_MASTER_HIRES.png`
  - `assets/ui/runtime/knobs/knob_reference_hires_56.png`
- Main band order now follows the approved reference: FREQ / GAIN / Q → DYNAMICS / ATTACK / RELEASE → DEGREE / LEVEL / MIX + ADV → AMOUNT / TYPE-A.
- UDMBC ATTACK / RELEASE and TRANSIENT remain available in ADVANCED so processing access is preserved.
- Ivory uses the uploaded 8×7 / 56-frame high-resolution knob runtime. Black retains the existing transparent locked sprite set because the uploaded high-resolution source contains an Ivory background field.
- DSP, parameter IDs, automation and processing order are unchanged.

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

## Remaining

**0**

## Next action

None for this migration. Any future UI change starts as a new Master-locked checkpoint rather than modifying the approved runtime in place without validation.
