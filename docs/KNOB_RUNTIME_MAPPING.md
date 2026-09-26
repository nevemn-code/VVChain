# High-resolution knob runtime mapping

## Approved master
`assets/ui/master/KNOB_MASTER_HIRES.png`

Keep this file unchanged.

## Engineering runtime asset
`assets/ui/runtime/knobs/knob_reference_hires_56.png`

The supplied knob master contains 56 populated positions arranged as 8 columns x 7 rows.
The lower unused region in the original source is intentionally not treated as knob frames.

Runtime mapping:
- columns = 8
- rows = 7
- frame count = 56
- normalized control value [0..1] -> round(value * 55)
- web background-size = 800% 700%
- native frame index = 0..55

Important:
- Do NOT directly overwrite the current 64-frame knob file and keep the old 8x8 logic.
- Either use the new 56-frame asset with 8x7 logic, or add per-asset frame metadata.
- v1.0.87: each source frame is registered to the same circle centre and radius in a regular 176×176 cell. The fixed tick marks and outer panel come from one frame; the rotating face is taken from the corresponding approved source frame.
- Before alignment the measured centre span was 5.609 px horizontally / 3.626 px vertically. After alignment it is 0.504 / 0.334 px; the radius span fell from 1.820 to 0.300 px.
- The unchanged approved `KNOB_MASTER_HIRES.png` remains the material source. Registration moves/samples existing pixels and does not add a new knob design.
- The original master remains the visual source of truth.

## v1.0.87 runtime selection

- Ivory: use the 56-frame high-resolution reference sprite.
- Black: retain the existing transparent locked Silver/Platinum sprites until a transparent black-safe derivative is approved.
