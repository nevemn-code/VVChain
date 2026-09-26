# UI3 Hardware Gold Candidate

Status: **USER-APPROVED VISUAL — AWAITING BYTE-EXACT MASTER INGEST / NOT PRODUCTION**

Branch: `ui/ui3-hardware-gold-candidate`

## Approved visual identity

The user explicitly approved the latest Hardware Gold / Amber render as the third UI Visual Master source.

Required immutable repository destination:

- `assets/ui/master/VVChain_hardware_gold_master.png`

Exact source identity:

- dimensions: **1492×1054**
- PNG source mode: **RGB**
- SHA-256: `5ae2b3e3d021123882503c227abcd8f7b5446cdf5b40b97fed491fc209f69631`

The approved image includes the requested Band 2 EQ row with three yellow knobs. Its control arrangement, material response, lighting, metal, bevel, AO, button styling, LED styling and proportions are not to be redesigned.

The binary is not yet committed to the repository. Per `UI_MASTER_LOCK.md`, extraction/state construction must stop at this gate until the exact binary is present and verified.

## Previous candidate engineering preview

The direct-composite approval preview was produced from the two supplied PNGs only.

- Preview: `VVChain_UI3_candidate_1586x992.png`
  - 1586×992
  - SHA-256 `810dcce9a8bdaaa4e450122df8408b1726cc76626bcc28d7ac3e80585c94e139`
- X4 working candidate: `VVChain_UI3_candidate_4x_6344x3968.png`
  - 6344×3968
  - SHA-256 `7c645b8d06b738f7b8f61023621d398d3aa4b094b5a6ab7ad023af4ae60a1094`
- X4 graph reference: `VVChain_UI3_spectrum_reference_4x_3756x792.png`
  - 3756×792
  - SHA-256 `69709d7f4fe273bc79093bdde4a98933b5695413ab976c012ec485a126c49607`

The approval-preview graph occupies source-panel pixels `x=117..1462`, `y=117..400`.

These binaries are intentionally **not** committed as approved runtime assets yet. The current GitHub connector can safely prepare the repository and code path, but the user-approved binary must first be accepted as the third Visual Master under `UI_MASTER_LOCK.md`.

## Production rule for the graph

The supplied graph screenshot contains visible EQ nodes and response curves. Those are reference appearance only.

Production UI3 must **not bake the live nodes/response curve into the graph background**, because Native and Web already draw live EQ, Dynamic EQ, analyzer, crossover markers, hover/SOLO state and gestures at runtime.

Production graph implementation after approval:

1. Extract/retain the dark graphite graph bed, dense logarithmic grid, warm-gold axis treatment and labels from the approved UI3 master.
2. Keep all live curves/nodes runtime-driven.
3. Match live EQ response to the supplied warm-gold line / warm translucent fill treatment.
4. Keep Dynamic EQ / X1-X3 / hover / SOLO behavior functionally identical to the current build.
5. Do not change DSP, APVTS parameter IDs, automation, preset compatibility, processing order or sound.

## Required repository integration after byte-exact Master ingest

1. Verify the committed file at `assets/ui/master/VVChain_hardware_gold_master.png` is exactly 1492×1054 and SHA-256 `5ae2b3e3d021123882503c227abcd8f7b5446cdf5b40b97fed491fc209f69631`.
2. Extend `master_manifest.json` and `UI_MASTER_LOCK.md` from 2 masters to 3.
3. Derive/validate UI3 runtime assets only from that committed approved Master.
4. Keep every animated knob frame on one identical pixel canvas and centre anchor. The knob body may not translate by even one pixel between states; only the pointer/state changes. Frame geometry mismatch = FAIL.
5. Validate each PNG group at 100% / 200% / 400%, overlay, colour/black level, highlight, AO/shadow, bevel, alpha edge and state geometry.
6. Commit each validated group immediately and update `WORK_PROGRESS.md` before starting the next group.
7. Extend Native theme state from the current Black/Ivory boolean to a 3-state UI skin selector.
8. Extend Web theme state to the same 3-state selector.
9. Native and Web must consume the same approved UI3 PNG set.
10. Theme switching is UI-only; no audio parameter, DSP, processing order, automation or preset compatibility may change.
11. Run Web smoke, UI regression, Master Lock, Native build and Pages parity gates before merge.
12. CI/CD may only Build / Test / Package / Deploy committed, validated PNGs and may not redraw or regenerate UI3 assets.

## Current production remains unchanged

Main stays on the approved v1.0.85 Black/Ivory Master runtime until the third candidate is explicitly approved.
