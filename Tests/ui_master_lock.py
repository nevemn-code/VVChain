#!/usr/bin/env python3
from __future__ import annotations
import hashlib, json, struct
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
MANIFEST=ROOT/"assets/ui/master/master_manifest.json"
STAGED=ROOT/"assets/ui/runtime/staged_manifest.json"
DERIVATIVE=ROOT/"assets/ui/master/approved_derivative_sources.json"
BINARY_MANIFEST=ROOT/"assets/ui/runtime/binary_manifest.json"
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
    assert STAGED.is_file()
    assert DERIVATIVE.is_file()
    assert BINARY_MANIFEST.is_file()
    assert (ROOT/"Tools/validate_master_runtime.py").is_file()
    assert (ROOT/"Tools/approve_master_runtime.py").is_file()
    for p in RENDERERS:
        assert not p.exists(), f"retired UI renderer returned: {p}"

    staged=json.loads(STAGED.read_text(encoding="utf-8"))
    derivative=json.loads(DERIVATIVE.read_text(encoding="utf-8"))
    binary=json.loads(BINARY_MANIFEST.read_text(encoding="utf-8"))
    assert staged["status"] in {"staged_locally_pending_binary_ingest","locked"}
    assert len(staged["knobs"])==8
    assert staged["knobs"]["knob_platinum_master_gain_64.png"]["scale_vs_normal"]==1.5
    legacy_knobs={k:v for k,v in staged["knobs"].items() if k!="knob_reference_hires_56.png"}
    assert all(v.get("outer_ring_max_diff")==0 for v in legacy_knobs.values())
    hires=staged["knobs"]["knob_reference_hires_56.png"]
    assert hires["columns"]==8 and hires["rows"]==7 and hires["frames"]==56
    assert hires["frame"]==176
    assert hires["sha256"]=="a55d1eb9e76160c4b528121d2e4f73e61f4567dce1b63a96c490a30b1b04b08e"
    assert hires["center_span_px"]==[0.504,0.334]
    assert derivative["status"]=="approved_source_identity_locked"
    assert binary["schema"]==1
    assert binary["asset_count"]==47==len(binary["assets"])
    assert binary["assets"]["knobs/knob_reference_hires_56.png"]==[
        1618708,
        "a55d1eb9e76160c4b528121d2e4f73e61f4567dce1b63a96c490a30b1b04b08e",
        1408,1232
    ]
    assert binary["source_archive"]["sha256"]=="1fca9eca20ccc1852310287fe41fbd59341aa018726c3ce764e934b2a7e6c297"

    m=json.loads(MANIFEST.read_text(encoding="utf-8"))
    assert m["status"] in {"pending_ingest","locked"}
    missing=[]
    scoped=m.get("scoped_masters",{})
    for name, spec in {**m["masters"], **scoped}.items():
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
