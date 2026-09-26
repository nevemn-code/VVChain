#!/usr/bin/env python3
from PIL import Image
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/ui/master/KNOB_MASTER_HIRES.png"
OUTPUT = ROOT / "assets/ui/runtime/knobs/knob_reference_hires_56.png"

X = [0, 164, 323, 484, 636, 786, 938, 1092, 1253]
Y = [1, 164, 328, 488, 648, 808, 969, 1131]
FRAME = 164

im = Image.open(SOURCE).convert("RGBA")
out = Image.new("RGBA", (8 * FRAME, 7 * FRAME), (0, 0, 0, 0))

for row in range(7):
    for col in range(8):
        cell = im.crop((X[col], Y[row], X[col + 1], Y[row + 1]))
        x = col * FRAME + (FRAME - cell.width) // 2
        y = row * FRAME + (FRAME - cell.height) // 2
        out.alpha_composite(cell, (x, y))

OUTPUT.parent.mkdir(parents=True, exist_ok=True)
out.save(OUTPUT, "PNG", optimize=False)
print(OUTPUT)
