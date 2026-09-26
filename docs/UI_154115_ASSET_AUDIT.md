# UI 154115 component audit

## Reference identity

- Active supplied Master: `assets/ui/master/VVChain_20260926_154115_master.png`
- 1456 × 1080, 8-bit RGBA, entirely opaque.
- SHA-256: `479bf03e2ff528ece86536d5d65f1aa77a54247e4cb1b25d2eff53969f3654b2`.

## Extraction candidate

The source-visible default state has been divided into 106 indexed components:

| Group | Individual parts | Source state |
| --- | ---: | --- |
| Rotary controls | 49 | One visible angle per control |
| Buttons, switches and slider | 38 | One visible state per control |
| LEDs | 5 | One visible state per light |
| Header, graph and panel crops | 7 | Contains baked text, graph and/or controls |
| Screws | 7 | One static view |

The review archive contains each part at its source dimensions and as a fourfold
Lanczos resample, the unchanged original reference, a fourfold panel resample,
manifest metadata, and a README. The archive is an engineering **Candidate**.
It is not committed to `assets/ui/runtime/` or included in Web/Native builds.

Candidate archive: `VVChain_154115_components_CANDIDATE.zip`, 43,402,687 bytes,
SHA-256 `533fc03ef5db2ac73a875f47e83bc62b5f98719b71711ab599872527037c770c`.

## Checks and limits

- PASS: all 106 extracted components retain the exact RGB source pixels wherever
  their alpha mask is fully opaque.
- PASS: 214 component PNGs and 2 panel PNGs decode; dimensions, RGBA mode,
  source hashes and ZIP integrity validate.
- FAIL for runtime approval: circular/rounded alpha boundaries include some
  source panel/刻度 pixels and have not been validated against a clean plate.
- FAIL for runtime approval: the source has no uncovered background beneath
  controls, intermediate rotary frames, or off/pressed/disabled states.
- FAIL for intrinsic fourfold detail: fourfold files contain interpolated
  source pixels, not a higher-resolution material render.
- FAIL for isolated panel/graph: those crops still contain baked text, curves,
  knobs and buttons. The graph must be separated from its dynamic plot.
- A generated high-resolution knob with thicker amber ring and different
  highlight/pointer was visually rejected and is absent from runtime.

## Next checkpoint

Acquire approved layer/angle/state sources or regenerate each missing state for
review against the Master. Until passing checks exist for all interaction states,
keep both Native and Web on the existing locked runtime and leave the product
version unchanged. CI/CD must not generate visual assets.
