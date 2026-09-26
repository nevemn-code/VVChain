# Integration checklist

## Recommended repository paths

assets/ui/master/
  UI_LAYOUT_MASTER_EXACT_ORDER.png
  KNOB_MASTER_HIRES.png

assets/ui/runtime/knobs/
  knob_reference_hires_56.png

docs/
  UI_LAYOUT_EXACT_ORDER.md
  KNOB_RUNTIME_MAPPING.md

tools/
  build_knob_reference_sprite.py

## Native JUCE changes

Current code uses an 8x8 / 64-frame assumption. For the approved new knob:

- columns: 8
- rows: 7
- frameCount: 56
- frame = round(sliderPosProportional * 55)

Best implementation: make frame geometry asset-specific so legacy 64-frame assets can remain available.

## Web changes

Current web constants are 8x8 / 64 frames.
For the new approved knob use:

- KNOB_SPRITE_COLUMNS = 8
- KNOB_SPRITE_ROWS = 7
- KNOB_FRAME_COUNT = 56
- background-size: 800% 700%

Do not change EQ/DSP behavior while doing this UI asset integration.

## Layout rule

The lower UI must follow the exact order shown in `UI_LAYOUT_MASTER_EXACT_ORDER.png`.
Do not use the current runtime layout as the authority if it conflicts with that image.
