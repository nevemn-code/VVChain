#!/usr/bin/env python3
"""Validation-only activation. This script never renders, recolours or generates UI pixels."""
from __future__ import annotations
import hashlib, json, struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "assets/ui/runtime"
MANIFEST = RUNTIME / "binary_manifest.json"
LOCK = RUNTIME / "APPROVED.lock"
MASTER_MANIFEST = ROOT / "assets/ui/master/master_manifest.json"
PNG_SIG = b"\x89PNG\r\n\x1a\n"

def check_png(path, expected):
    data = path.read_bytes()
    assert data[:8] == PNG_SIG, path
    w,h = struct.unpack(">II", data[16:24])
    assert len(data) == expected[0], (path, len(data), expected[0])
    assert hashlib.sha256(data).hexdigest() == expected[1], path
    assert (w,h) == (expected[2], expected[3]), (path,w,h)
    assert data[24] == 8 and data[25] == 6, (path, data[24], data[25])

runtime_bytes = MANIFEST.read_bytes()
runtime = json.loads(runtime_bytes.decode("utf-8"))
master = json.loads(MASTER_MANIFEST.read_text(encoding="utf-8"))

for name, spec in master["masters"].items():
    p = ROOT / spec["path"]
    data = p.read_bytes()
    assert data[:8] == PNG_SIG, p
    assert len(data) == spec["bytes"], p
    assert hashlib.sha256(data).hexdigest() == spec["sha256"], p

for rel, spec in runtime["assets"].items():
    p = RUNTIME / rel
    assert p.is_file(), f"missing runtime asset: {rel}"
    check_png(p, spec)

LOCK.write_text(json.dumps({
    "schema": 1,
    "runtime_manifest_sha256": hashlib.sha256(runtime_bytes).hexdigest(),
    "asset_count": runtime["asset_count"],
    "master_black_sha256": master["masters"]["black"]["sha256"],
    "master_ivory_sha256": master["masters"]["ivory"]["sha256"],
    "note": "Created only after byte-exact validation; does not generate visual assets."
}, indent=2) + "\n", encoding="utf-8")
print("APPROVED.lock created after byte-exact Master/runtime validation")
