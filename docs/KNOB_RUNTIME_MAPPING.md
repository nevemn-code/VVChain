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
- The engineering sprite keeps source knob pixels 1:1 and only center-pads cells to a regular 164x164 grid.
- The original master remains the visual source of truth.
