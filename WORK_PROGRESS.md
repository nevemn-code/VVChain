# VVChain Work Progress

## Goal

Replace the rejected/approximate VVChain UI asset pipeline with a Master-locked PNG workflow, then migrate Native and Web to one shared set of Master-validated runtime assets without changing DSP, parameter IDs, automation, mouse behaviour, processing order or preset compatibility.

## Current version

v1.0.84

## Current checkpoint

**v1.0.84 — Third-reference layout alignment; approved PNG binaries unchanged**

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
