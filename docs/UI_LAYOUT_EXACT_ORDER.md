# VVChain exact UI ordering master

## Single source of truth
`assets/ui/master/UI_LAYOUT_MASTER_EXACT_ORDER.png`

The approved composition is **1265 × 938**. Runtime controls may be interactive overlays, but their visible geometry must match this coordinate contract.

## Pixel coordinate contract

- Full UI: `1265 × 938`
- Header: `y 0–84`
- Graph outer viewport: `x 28, y 86, w 1212, h 260`
- Lower strip: `x 36, y 351, w 1204, h 570`
- Column gaps: `8 px`
- BAND widths: `236 px` each
- MASTER width: `228 px`
- Column x origins: `36, 280, 524, 768, 1012`

### Band control centres
Each BAND uses the same local x centres:
- column 1: `x + 51`
- column 2: `x + 119`
- column 3: `x + 187`

Measured global centres for BAND 1 are approximately `87 / 155 / 223`; every following band is offset by 244 px.

### Band row lanes
Relative to `cardY = 351`:
- EQ row: `+58`
- Dynamic EQ row: `+179`
- UDMBC row: `+299`
- Analog row: `+423`
- SOLO / BYPASS row: `+516`

## Each BAND column, top to bottom

1. Band badge + `BAND n` + frequency class.
2. `EQ`: **FREQ / GAIN / Q**.
3. `DYNAMIC EQ`: **THRESH / ATTACK / RELEASE**.
4. `UDMBC`: **DEGREE / LEVEL / MIX + ADV**.
5. `ANALOG`: **TT / SS** over **AMOUNT / TYPEA / TRANSIENT**.
6. Bottom buttons: **SOLO / BYPASS**.

## MASTER column

Reference geometry:
- MIX centre: approximately `(1069, 483)`
- OUT centre: approximately `(1174, 483)`
- DELTA: `x 1027, y 554, w 94, h 35`
- BYPASS: `x 1129, y 554, w 96, h 35`
- SOLO MATRIX occupies the next two rows.
- X-OVER / GLOBAL and PRE / POST stay in the lower MASTER section.

## Header hit zones

- BYPASS 1: `x 862, y 37, w 106, h 34`
- BYPASS 2: `x 987, y 37, w 106, h 34`
- Settings: `x 1113, y 37, w 70, h 34`

Do not move, add, remove, or reorder visible controls in the main strip without a new approved Master.
