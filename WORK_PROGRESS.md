# VVChain Work Progress

## Goal

Replace the rejected/approximate VVChain UI asset pipeline with a Master-locked PNG workflow, then migrate Native and Web to one shared set of Master-validated runtime assets without changing DSP, parameter IDs, automation, mouse behaviour, processing order or preset compatibility.

## Current version

v1.0.78

## Current checkpoint

**Checkpoint 7 — Static-audit recovery + CI revalidation**

## Master paths

Required exact original binaries:

- `assets/ui/master/VVChain_black_master.png`
- `assets/ui/master/VVChain_ivory_master.png`

## Completed in this checkpoint

- Created permanent UI Master Lock policy.
- Declared the two approved original UI PNGs as the only Visual Masters.
- Retired the repository Python UI PNG renderer from the active source tree.
- Recorded that existing `docs/assets/ui/png/` assets remain unapproved Candidates until Master comparison.
- Added resumable checkpoint state.
- Bumped repository-visible version references to v1.0.72 for the initial GitHub change batch.\n- Recorded byte-exact SHA-256, dimensions, byte counts and RGBA mode for both user-approved Master PNGs in `assets/ui/master/master_manifest.json`.\n- Added `Tests/ui_master_lock.py` so future CI cannot silently reintroduce the retired renderer or accept a wrong Master binary.\n- Added explicit runtime and rejected/candidate asset areas.\n- VVChain Fast CI/CD #1328 completed successfully for Checkpoint 1.\n- VVChain Fast CI/CD #1329 completed successfully for Checkpoint 2, including the UI Master Lock gate.\n- Locked exact identities of all user-approved panel/button/icon/knob derivative source sheets.\n- Staged seven engineered knob sprites locally: six metal colours at 128 px/frame and platinum Master Gain at 192 px/frame (1.5x).\n- Rebuilt knob state sequences so the metal body and amber outer LED ring are fixed across all 64 states; measured outer-ring max pixel delta = 0 for every colour.\n- Pointer motion is 64 states over -125°..+125°, constructed at 4x sampling then downsampled.\n- Staged direct-crop RGBA button/square/toggle/LED/screw assets from the approved source sheets; no procedural redraw.
- Added a Native Master runtime integration bridge guarded by `assets/ui/runtime/APPROVED.lock`; without the lock, the existing Candidate runtime remains active and buildable.
- CMake now refuses an incomplete Master runtime if APPROVED.lock exists but any required asset is missing.
- Prepared Native loading for the full Black/Ivory approved panel backgrounds, six colour knob sprites, 1.5x platinum Master Gain sprite, approved buttons/LEDs/screws and slider assets.
- Knob selection is parameter-ID based: EQ=silver, Dynamic EQ=blue, UDMBC=green, Analog=gold, Tape=red, Transient/XOVER=black, OUTPUT_LEVEL=1.5x platinum.
- Native sprite reader now supports both 128 px normal frames and 192 px Master Gain frames instead of hard-coding 128.
- When the Master runtime becomes approved, Native editor geometry switches to the 1470x1070 Master panel basis while preserving DSP/automation/parameter IDs.
- Fixed the stale static-audit expectation that incorrectly required 128 px for every sprite frame; the renderer now intentionally supports 128 px normal and 192 px platinum Master Gain frames.
- Web runtime now has an APPROVED.lock gate matching Native. Without the lock, current Candidate assets remain active.
- When approved, Web switches to the same 1470x1070 Master panel basis, full Black/Ivory backgrounds, six colour knob sprites, 1.5x platinum OUTPUT knob, approved buttons/LEDs/toggles/sliders, and responsive whole-panel scaling.
- Web knob colour selection is parameter-ID based with the same Native mapping; Dynamic EQ/UDMBC/Analog/Tape/Transient controls now carry explicit IDs where needed.
- VVChain Fast CI/CD #1332 exposed one Web-only integration defect: literal `\\n` characters were inserted into the `drawEQ()` Master-runtime guard, causing Node syntax validation to fail.
- Replaced those escaped characters with real JavaScript newlines. No DSP, parameter, automation or interaction logic changed.
- The earlier v1.0.75 stale 128-px static-audit assertion was already corrected to permit the approved 192-px platinum Master Gain sprite.
- VVChain Fast CI/CD #1333 confirmed Web smoke ×10 passes at v1.0.77; it then exposed literal `\\n` tokens accidentally embedded in two new static-audit assertion lines.
- Replaced only those malformed assertion separators with real Python newlines; the PNG-signature escape string remains intentionally unchanged.

## Accepted runtime assets

None yet under the new Master Lock process.

## Rejected / unapproved assets

All current assets under `docs/assets/ui/png/` are treated as Candidate/unapproved for Master fidelity until individually validated.

They remain temporarily in place only to avoid breaking Native/Web before the exact Master binaries and replacement runtime set are committed.

## Remaining

1. Commit the exact original black Master PNG binary.
2. Commit the exact original ivory Master PNG binary.
3. Verify byte-level PNG validity and record hashes/dimensions.
4. Derive/crop/clean the required engineering assets from the actual Masters only.
5. Validate every asset group against the Master.
6. Move only PASS assets into the production runtime set.
7. Wire Native and Web to the same approved runtime files.
8. Remove rejected Candidate runtime files only after replacements are present.
9. Validate Black UI, Ivory UI, knob colour set, 1.5x platinum Master Gain knob, bypass states, scaling, high-DPI, Native and Web.
10. Run CI/CD and set Remaining to 0 only after all checks pass.

## Safety / compatibility constraint

Do not delete the currently referenced runtime PNG files until their approved replacements exist in the repository and the corresponding Native/Web references can be switched atomically. This prevents a broken half-migration.

## Next action

Ingest the two exact user-approved Master PNG binaries into `assets/ui/master/`. Do not regenerate substitutes.
