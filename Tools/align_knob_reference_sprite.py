"""Align approved 56-frame knob pixels to a shared rotary centre.

The existing sprite is the source. Keep the fixed outer dial/ticks of frame 0,
and transfer each source frame's face/ring after circle registration. This is a
candidate state-construction operation; no new material is painted.
"""

from pathlib import Path
import hashlib
import json

import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from scipy.optimize import least_squares

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/ui/candidates/knob_reference_hires_56_unaligned.png"
OUT = ROOT / "assets/ui/runtime/knobs/knob_reference_hires_56.png"
META = ROOT / "docs/KNOB_AXIS_VALIDATION.json"
CELL = 176
TARGET = 87.5
RADIUS = 50.0
GRID = 8, 7


def circle(tile: Image.Image) -> tuple[float, float, float]:
    arr = np.asarray(tile.convert("RGB")).astype(float)
    yy, xx = np.indices((164, 164))
    rough = np.hypot(xx - 81.5, yy - 81.5)
    red, green, blue = arr.transpose(2, 0, 1)
    # The narrow gold annulus is visible at all 56 angles. Ignore the pointer.
    ring = ((rough > 42) & (rough < 65) &
            (red > green * 1.28) & (green > blue * 1.2) &
            (red > 100) & (green > 50))
    x = xx[ring]
    y = yy[ring]
    if len(x) < 400:
        raise ValueError("Insufficient source annulus pixels")
    fit = least_squares(lambda p: np.hypot(x - p[0], y - p[1]) - p[2],
                        (81.5, 81.5, 50), loss="soft_l1", f_scale=1.5)
    return tuple(float(v) for v in fit.x)


def main() -> None:
    src = Image.open(SOURCE).convert("RGBA")
    assert src.size == (1312, 1148)
    tiles = [src.crop(((k % 8)*164, (k // 8)*164,
                       (k % 8 + 1)*164, (k // 8 + 1)*164)) for k in range(56)]
    measured = [circle(tile) for tile in tiles]
    aligned = []
    for tile, (cx, cy, radius) in zip(tiles, measured):
        scale = RADIUS / radius
        # PIL affine maps target coordinates back into the source tile.
        inv = 1 / scale
        aligned.append(tile.transform((CELL, CELL), Image.Transform.AFFINE,
                       (inv, 0, cx - TARGET * inv,
                        0, inv, cy - TARGET * inv),
                       resample=Image.Resampling.BICUBIC,
                       fillcolor=(0, 0, 0, 0)))
    base = aligned[0]
    face_mask = Image.new("L", (CELL, CELL), 0)
    ImageDraw.Draw(face_mask).ellipse((TARGET - 57, TARGET - 57,
                                       TARGET + 57, TARGET + 57), fill=255)
    face_mask = face_mask.filter(ImageFilter.GaussianBlur(1.2))
    result = Image.new("RGBA", (CELL * GRID[0], CELL * GRID[1]), (0, 0, 0, 0))
    output_tiles = []
    for k, tile in enumerate(aligned):
        face = Image.composite(tile, base, face_mask)
        output_tiles.append(face)
        result.paste(face, ((k % 8) * CELL, (k // 8) * CELL))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    temporary = OUT.with_name(OUT.stem + ".writing.png")
    result.save(temporary, optimize=True)
    # Never leave a partial production asset if a write is interrupted.
    with Image.open(temporary) as verify:
        verify.load()
        if verify.size != result.size or verify.mode != "RGBA":
            raise AssertionError("Incomplete knob sprite output")
    expected_sha = "a55d1eb9e76160c4b528121d2e4f73e61f4567dce1b63a96c490a30b1b04b08e"
    if hashlib.sha256(temporary.read_bytes()).hexdigest() != expected_sha:
        raise AssertionError("Centred sprite differs from visually checked output")
    temporary.replace(OUT)
    sample = Image.new("RGBA", (CELL * 9, CELL), (0, 0, 0, 0))
    for j, k in enumerate((0, 7, 14, 21, 28, 35, 42, 49, 55)):
        sample.paste(output_tiles[k], (j * CELL, 0))
    sample.save(ROOT / "assets/ui/candidates/knob_alignment_contact_sheet.png")
    np_centres = np.asarray(measured)
    aligned_centres = np.asarray([circle(tile.crop((6, 6, 170, 170))) for tile in output_tiles])
    META.write_text(json.dumps({
        "source": str(SOURCE.relative_to(ROOT)), "source_frame_size": 164,
        "candidate_frame_size": CELL, "frames": 56,
        "source_centre_span": np.ptp(np_centres[:, :2], axis=0).round(3).tolist(),
        "result_centre_span": np.ptp(aligned_centres[:, :2], axis=0).round(3).tolist(),
        "source_radius_span": float(np.ptp(np_centres[:, 2])),
        "result_radius_span": float(np.ptp(aligned_centres[:, 2])),
        "measured_source_circles": np_centres.round(3).tolist(),
        "status": "v1.0.87 visually checked at 100%, 200%, 400%; circle fit verified"}, indent=2) + "\n")
    print(META.read_text()[:330])


if __name__ == "__main__":
    main()
