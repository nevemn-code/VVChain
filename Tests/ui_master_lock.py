#!/usr/bin/env python3
from __future__ import annotations
import hashlib, json, struct
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
MANIFEST=ROOT/"assets/ui/master/master_manifest.json"
RENDERERS=[
    ROOT/"Tools/generate_ui_png_assets.py",
    ROOT/"Tools/generate_ui_assets.py",
]

def png_info(path: Path):
    data=path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise AssertionError(f"{path}: invalid PNG signature")
    w,h=struct.unpack(">II",data[16:24])
    bit_depth=data[24]
    colour_type=data[25]
    return data,w,h,bit_depth,colour_type

def main():
    assert (ROOT/"UI_MASTER_LOCK.md").is_file()
    assert (ROOT/"WORK_PROGRESS.md").is_file()
    assert MANIFEST.is_file()
    for p in RENDERERS:
        assert not p.exists(), f"retired UI renderer returned: {p}"

    m=json.loads(MANIFEST.read_text(encoding="utf-8"))
    assert m["status"] in {"pending_ingest","locked"}
    missing=[]
    for name, spec in m["masters"].items():
        p=ROOT/spec["path"]
        if not p.exists():
            missing.append(str(p.relative_to(ROOT)))
            continue
        data,w,h,bit_depth,colour_type=png_info(p)
        assert (w,h)==(spec["width"],spec["height"]), (name,w,h)
        assert len(data)==spec["bytes"], (name,len(data),spec["bytes"])
        assert hashlib.sha256(data).hexdigest()==spec["sha256"], name
        assert bit_depth==8, (name,bit_depth)
        assert colour_type==6, (name,"expected RGBA PNG colour type 6",colour_type)

    if m["status"]=="locked":
        assert not missing, "locked manifest missing masters: "+", ".join(missing)
    if missing:
        print("UI Master Lock: PENDING exact Master ingest:", ", ".join(missing))
    else:
        print("UI Master Lock: exact Master hashes PASS")

if __name__=="__main__":
    main()
