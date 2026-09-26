# VVChain Work Progress

## Goal

Replace the rejected/approximate VVChain UI asset pipeline with a Master-locked PNG workflow, then migrate Native and Web to one shared set of Master-validated runtime assets without changing DSP, parameter IDs, automation, mouse behaviour, processing order or preset compatibility.

## Current version

v1.0.81

## Current checkpoint

**Checkpoint 10 — Byte-exact runtime integrity contract**

## Master paths

Required exact original Visual Master binaries:

- `assets/ui/master/VVChain_black_master.png`
- `assets/ui/master/VVChain_ivory_master.png`

Their locked identities remain:

- Black: 1448×1086 RGBA, 3,012,513 bytes, SHA-256 `173d086d919d0a0b5c0c3100a060dc1929a918f9c1f8e4328424cb2e5e817b38`
- Ivory: 1448×1086 RGBA, 3,115,458 bytes, SHA-256 `9f99da776c8f081abd512bae269c79e47524c011cde190a56c2172ec9957effb`

## Completed checkpoints

- Master Asset Lock policy is permanent and is referenced by project rules.
- The old Python/procedural UI renderer has been retired from the repository.
- Existing `docs/assets/ui/png/` files are Candidate-only and cannot be treated as Master-faithful.
- Exact Visual Master identities are locked in `assets/ui/master/master_manifest.json`.
- Approved panel/button/icon/knob derivative source identities are locked separately and do not supersede the full Masters.
- Six normal metal knob colours are staged: Gold, Blue, Green, Red, Black and Silver.
- Platinum Master Gain is staged at 1.5× normal knob size.
- All knob sprites use 64 states; the body and amber outer LED ring remain fixed and only the pointer state moves.
- Knob engineering used X4 sampling before final downsampling.
- Native Master runtime bridge is guarded by `assets/ui/runtime/APPROVED.lock`.
- Web Master runtime uses the same approval sentinel and the same source runtime files.
- Native/Web parameter-ID knob mapping is aligned: EQ=silver, Dynamic EQ=blue, UDMBC=green, Analog=gold, Tape=red, Transient/XOVER=black, OUTPUT_LEVEL=platinum 1.5×.
- Native Master geometry is aligned to the approved 1470×1070 engineering panel basis.
- Old Candidate graph/module textures, duplicate procedural frames and duplicate screws are suppressed when the Master runtime is active.
- Pages deployment copies the approved root runtime byte-for-byte; it does not redraw or regenerate UI assets.
- Fast CI/CD #1334 passed all Fast Gate checks after the syntax/static-audit corrections.
- Fast CI/CD #1336 passed completely: Fast Gate, Windows VST3 Release and Full Native/DSP Validation all succeeded.
- Added `assets/ui/runtime/binary_manifest.json` covering all 46 approved runtime PNGs with byte count, SHA-256 and exact dimensions.
- Locked source archive identity: `VVChain_UI_Runtime_Engineering_v1.0.74.zip`, 7,322,749 bytes, SHA-256 `1fca9eca20ccc1852310287fe41fbd59341aa018726c3ce764e934b2a7e6c297`.
- Added `Tools/validate_master_runtime.py`. It validates existing runtime files and becomes strict when APPROVED.lock exists.
- Added `Tools/approve_master_runtime.py`. It is validation-only: it never renders pixels and can create APPROVED.lock only after the exact Masters and every runtime PNG pass byte/hash/dimension/RGBA checks.
- CI and Pages are wired to the runtime integrity validator.

## Accepted runtime assets

The visual/engineering identities of the 46 staged runtime PNGs are locked in the binary manifest, but they are **not yet active in GitHub runtime** because the binary files have not all been committed and APPROVED.lock has not been created.

## Rejected / unapproved assets

All current legacy files under `docs/assets/ui/png/` remain Candidate/unapproved for Master fidelity. They stay temporarily only to keep Native/Web runnable until the approved binary replacement can be activated atomically.

## Remaining

1. Commit the two exact original Visual Master PNG binaries.
2. Commit all 46 exact runtime PNG binaries matching `binary_manifest.json`.
3. Run `Tests/ui_master_lock.py` and `Tools/validate_master_runtime.py`.
4. Perform final visual acceptance against both Masters.
5. Run `Tools/approve_master_runtime.py` to create APPROVED.lock only after PASS.
6. Verify Native and Web both activate the same runtime PNG source.
7. Remove the rejected Candidate runtime after the approved path is confirmed.
8. Run final Black/Ivory, six-colour knob, platinum 1.5×, bypass, high-DPI, Web and VST3 validation.
9. Set Remaining to 0 only after the final approved build/deploy passes.

## Safety / compatibility constraint

Never create APPROVED.lock while any binary is missing, mismatched, substituted or merely similar. Never delete the currently referenced Candidate runtime before the approved replacement is present and validated.

## Next action

Ingest the exact Master/runtime binary PNGs. The repo is now prepared to reject a partial or wrong binary set automatically.
