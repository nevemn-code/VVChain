# VVChain UI Asset Map

## Authority

The two full Master PNGs remain the only Visual Masters. Approved derivative source sheets are locked by SHA-256 in `assets/ui/master/approved_derivative_sources.json` and may only be used for direct extraction / state construction permitted by `UI_MASTER_LOCK.md`.

## Knob assignment

### v1.0.84 reference presentation

The supplied third reference uses one neutral hardware family across the four band strips. Production selection therefore reuses the existing approved assets without editing them:

- Metal Silver — all normal band controls, including EQ, Dynamic EQ, UDMBC, Analog, Tape and Transient.
- Platinum — MASTER output class only, 1.5× normal knob size.

The already-approved Gold, Blue, Green, Red and Black sprites remain in the locked runtime set for future approved layouts, but v1.0.84 does not select them by default.

All knob variants preserve the same amber outer LED ring. Runtime state sequences use a fixed body and only move the pointer.

## Runtime target

Normal knob sprite: 8×8, 64 frames, 128×128 per frame, 1024×1024 sheet.
Master Gain: 8×8, 64 frames, 192×192 per frame, 1536×1536 sheet.

## Buttons / indicators

Black and Ivory each use separate OFF / ON / PRESSED / DISABLED raster assets. Disabled states are real PNG assets, not CSS/runtime grayscale.

LEDs use dedicated OFF / ON assets, plus dedicated red bypass and status colours.

## Native / Web

Both implementations must consume the same approved runtime source files. No independent redraw, SVG replacement, CSS imitation or Canvas reconstruction is allowed.
