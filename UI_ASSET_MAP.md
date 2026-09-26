# VVChain UI Asset Map

## Authority

The two full Master PNGs remain the only Visual Masters. Approved derivative source sheets are locked by SHA-256 in `assets/ui/master/approved_derivative_sources.json` and may only be used for direct extraction / state construction permitted by `UI_MASTER_LOCK.md`.

## Knob assignment

- Metal Gold — general warm/analog controls where gold is the functional colour.
- Metal Blue — blue-coded functions.
- Metal Green — green-coded functions.
- Metal Red — red-coded functions.
- Metal Black — neutral/dark functions.
- Metal Silver — neutral precision/EQ controls.
- Platinum — MASTER gain/output class only, 1.5× normal knob size.

All knob variants preserve the same amber outer LED ring. Runtime state sequences use a fixed body and only move the pointer.

## Runtime target

Normal knob sprite: 8×8, 64 frames, 128×128 per frame, 1024×1024 sheet.
Master Gain: 8×8, 64 frames, 192×192 per frame, 1536×1536 sheet.

## Buttons / indicators

Black and Ivory each use separate OFF / ON / PRESSED / DISABLED raster assets. Disabled states are real PNG assets, not CSS/runtime grayscale.

LEDs use dedicated OFF / ON assets, plus dedicated red bypass and status colours.

## Native / Web

Both implementations must consume the same approved runtime source files. No independent redraw, SVG replacement, CSS imitation or Canvas reconstruction is allowed.
