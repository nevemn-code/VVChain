#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "assets/ui/runtime"
MANIFEST = RUNTIME / "binary_manifest.json"
LOCK = RUNTIME / "APPROVED.lock"

PNG_SIG = b"\x89PNG\r\n\x1a\n"

def load_manifest():
    data = MANIFEST.read_bytes()
    obj = json.loads(data.decode("utf-8"))
    assert obj["schema"] == 1
    assert obj["asset_count"] == len(obj["assets"]) == 46
    assert obj["png_contract"]["bit_depth"] == 8
    assert obj["png_contract"]["colour_type"] == 6
    return obj, hashlib.sha256(data).hexdigest()

def validate_asset(path: Path, spec):
    data = path.read_bytes()
    assert data[:8] == PNG_SIG, f"{path}: PNG signature"
    width, height = struct.unpack(">II", data[16:24])
    bit_depth = data[24]
    colour_type = data[25]
    expected_bytes, expected_sha, expected_w, expected_h = spec
    assert len(data) == expected_bytes, (path, len(data), expected_bytes)
    assert hashlib.sha256(data).hexdigest() == expected_sha, f"{path}: SHA-256 mismatch"
    assert (width, height) == (expected_w, expected_h), (path, width, height)
    assert bit_depth == 8, (path, "bit depth", bit_depth)
    assert colour_type == 6, (path, "RGBA colour type required", colour_type)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--require-approved", action="store_true")
    args = ap.parse_args()

    obj, manifest_sha = load_manifest()
    missing = []
    for rel, spec in obj["assets"].items():
        p = RUNTIME / rel
        if not p.exists():
            missing.append(rel)
            continue
        validate_asset(p, spec)

    if LOCK.exists():
        lock = json.loads(LOCK.read_text(encoding="utf-8"))
        assert lock["schema"] == 1
        assert lock["runtime_manifest_sha256"] == manifest_sha
        assert lock["asset_count"] == 46
        assert not missing, "APPROVED.lock present but files missing: " + ", ".join(missing)
        print("Master runtime: APPROVED + binary hashes PASS")
        return

    if args.require_approved:
        raise AssertionError("APPROVED.lock is required but missing")

    if missing:
        print(f"Master runtime: pending binary ingest ({len(missing)} / 46 missing)")
    else:
        print("Master runtime: all binaries present but not approved; run approval only after visual acceptance")

if __name__ == "__main__":
    main()
