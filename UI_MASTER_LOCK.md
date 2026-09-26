# VVChain UI Master Asset Lock

## Status

This policy is effective from **v1.0.72** and is the highest-priority rule for VVChain visual asset work.

## Single Source of Truth

The user-approved original full-panel PNGs remain the surface/skin Visual Masters.
Two scoped masters are also locked for v1.0.86:
- `assets/ui/master/UI_LAYOUT_MASTER_EXACT_ORDER.png` controls component ordering/grouping only.
- `assets/ui/master/KNOB_MASTER_HIRES.png` controls the Ivory rotary-knob appearance only.

When a scoped master conflicts with an older full-panel baked control position or older knob sprite, the scoped master wins only for its declared scope; panel material/lighting still comes from the full-panel Master.

Expected immutable master paths:

- `assets/ui/master/VVChain_black_master.png`
- `assets/ui/master/VVChain_ivory_master.png`

Until both exact original Master PNG binaries exist at those paths, no replacement runtime asset may be described as complete, identical, 100% restored, or production-approved.

## Prohibited reconstruction

Do not rebuild the approved UI from descriptions or from approximate rendering systems. In particular, the approved UI must not be recreated from:

- SVG
- CSS drawing
- Canvas drawing
- Python renderers
- procedural renderers
- deterministic asset generators
- AI re-imagination from text
- recolouring a rejected candidate and calling it a Master derivative

If an existing asset differs from the Master, do not keep tuning a renderer and do not create an Nth approximate version. Return to the Master itself.

## Allowed engineering operations

Runtime assets may only be derived from the actual Master through controlled engineering operations such as:

- extraction / cropping
- alpha cleanup
- edge cleanup
- lossless cleanup
- state extraction
- state construction that preserves the same material, lighting, bevel, AO, shadow, geometry and visual identity
- high-quality downsampling from an approved high-resolution source

These operations must not redesign material, lighting, metal response, highlight placement, shadow, bevel, AO, LED design, knob geometry or proportions.

## Candidate vs Runtime

Existing PNGs that have not passed comparison to the Master are **Candidate** assets only.

Candidate assets must not be treated as the visual source of truth.

Only assets that have passed Master validation may enter the production runtime set.

## Visual validation

Every asset group must be compared against the Master before runtime acceptance.

Minimum checks:

1. 100% comparison
2. 200% comparison
3. 400% comparison when edge/material detail is relevant
4. overlay comparison where geometrically applicable
5. colour / black level comparison
6. highlight direction and intensity
7. shadow and AO
8. bevel and border thickness
9. alpha-edge quality
10. state-to-state geometry consistency

Result vocabulary is limited to **PASS** or **FAIL** for Master acceptance.

Terms such as "close enough", "similar", "optimised" or "same style" are not acceptance criteria.

## Native / Web parity

Native and Web must consume the same approved runtime PNG source assets.

Do not maintain separate approximate Native and Web visual sets.

No SVG/CSS/Canvas fallback may silently replace a missing approved runtime PNG.

## CI/CD rule

CI/CD may Build, Test, Validate, Package and Deploy committed, approved assets.

CI/CD must not generate, redraw, recolour or regenerate the approved UI.

A renderer is never the source of truth for the approved Master.

## Long-task checkpoint protocol

Long UI work must be resumable.

After each independently validated asset group:

1. write the real files
2. inspect the diff
3. validate
4. commit
5. confirm the remote commit
6. update `WORK_PROGRESS.md`

After interruption, read the Master paths, `UI_MASTER_LOCK.md`, `WORK_PROGRESS.md`, Git log, current diff and runtime assets before continuing.

Already accepted assets must not be regenerated merely because the conversation was interrupted.
