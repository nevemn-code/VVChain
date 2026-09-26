# UI3 Hardware Gold Candidate

Status: **CANDIDATE — NOT MASTER-LOCKED / NOT PRODUCTION**

Branch: `ui/ui3-hardware-gold-candidate`

## User supplied visual sources

- Hardware panel source: 1586×992 RGBA PNG.
- EQ / Dynamic EQ graph reference: 939×198 RGBA PNG.
- Requested change: keep the hardware panel/control arrangement and replace the upper graph visual language with the supplied dark/gold EQ graph reference.

## Candidate engineering preview

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

## Required repository integration after user approval

1. Add third immutable master, proposed path:
   - `assets/ui/master/VVChain_hardware_gold_master.png`
2. Extend `master_manifest.json` and `UI_MASTER_LOCK.md` from 2 masters to 3.
3. Derive/validate UI3 runtime assets only from the approved third master.
4. Extend Native theme state from the current Black/Ivory boolean to a 3-state UI skin selector.
5. Extend Web theme state to the same 3-state selector.
6. Native and Web must consume the same approved UI3 PNG set.
7. Theme switching is UI-only; no audio parameters may change.
8. Validate 100% / 200% / 400%, overlay, colour/black level, highlight, AO/shadow, bevel, alpha edge and state geometry.
9. Run Web smoke, UI regression, Master Lock, Native build and Pages parity gates before merge.

## Current production remains unchanged

Main stays on the approved v1.0.85 Black/Ivory Master runtime until the third candidate is explicitly approved.
