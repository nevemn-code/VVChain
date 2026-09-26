# VVChain UI Asset Map

## Authority

The two full Master PNGs remain the only Visual Masters. Approved derivative source sheets are locked by SHA-256 in `assets/ui/master/approved_derivative_sources.json` and may only be used for direct extraction / state construction permitted by `UI_MASTER_LOCK.md`.

## Knob assignment

### v1.0.86 reference presentation

The supplied third reference uses one neutral hardware family across the four band strips. Production selection therefore reuses the existing approved assets without editing them:

- Ivory runtime — `knob_reference_hires_56.png`, directly derived from the user-approved `KNOB_MASTER_HIRES.png`; 8×7 / 56 frames / 164×164 engineering cells.
- Black runtime — existing Silver/Platinum transparent locked sprites remain active until a black-safe transparent derivative of the new high-resolution master is approved.

The older Gold, Blue, Green, Red and Black sprites remain in the locked runtime set for compatibility and future approved layouts.

## Runtime target

Legacy transparent knob sprites: 8×8, 64 frames.
Ivory high-resolution reference knob: 8×7, 56 frames, 164×164 engineering cells, 1312×1148 sheet.
Master Gain legacy fallback: 8×8, 64 frames, 192×192 per frame, 1536×1536 sheet.

## Buttons / indicators

Black and Ivory each use separate OFF / ON / PRESSED / DISABLED raster assets. Disabled states are real PNG assets, not CSS/runtime grayscale.

LEDs use dedicated OFF / ON assets, plus dedicated red bypass and status colours.

## Native / Web

Both implementations must consume the same approved runtime source files. No independent redraw, SVG replacement, CSS imitation or Canvas reconstruction is allowed.
