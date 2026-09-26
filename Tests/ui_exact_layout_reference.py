from pathlib import Path

cpp = Path("Source/PluginEditor.cpp").read_text(encoding="utf-8")
web = Path("docs/index.html").read_text(encoding="utf-8")
cmake = Path("CMakeLists.txt").read_text(encoding="utf-8")

# Exact target frame from OKK(2).png
assert "project(VVChain VERSION 1.0.87" in cmake
assert "setSize(1265, 938);" in cpp
assert "return { 28.f, 86.f, 1212.f, 260.f };" in cpp

# Header: BYPASS / BYPASS / gear
for token in [
    "masterRuntime ? 862 : 0",
    "masterRuntime ? 987 : 0",
    "masterRuntime ? 1112 : w - 100",
    "constexpr int topY = 38",
]:
    assert token in cpp, token

for token in [
    "left:862px!important;top:38px!important;width:105px!important;height:35px",
    "left:987px!important;top:38px!important;width:105px!important;height:35px",
    "left:1112px!important;top:38px!important;width:69px!important;height:35px",
]:
    assert token in web, token

# Lower layout: BAND 1..4 + MASTER
for token in [
    "const int cardY = masterRuntime ? 351 : 404;",
    "const int cardW = masterRuntime",
    "masterRuntime ? 236",
    "masterRuntime ? 570",
    "grid-template-columns:236px 236px 236px 236px 228px",
]:
    assert token in (cpp + web), token

# Exact row ordering
for token in [
    'placeKnob("EQ" + n + "_FREQ", cell(0, 0));',
    'placeKnob("EQ" + n + "_GAIN", cell(0, 1));',
    'placeKnob("EQ" + n + "_Q",    cell(0, 2));',
    'placeKnob("DYN_DYNAMICS" + n, cell(1, 0));',
    'placeKnob("DYN_ATTACK" + n,   cell(1, 1));',
    'placeKnob("DYN_RELEASE" + n,  cell(1, 2));',
    'placeKnob("UDMBC_DEGREE" + n, cell(3, 0));',
    'placeKnob("UDMBC_LEVEL" + n,  cell(3, 1));',
    'placeKnob("UDMBC_COMP_M" + n, cell(3, 2));',
    'placeKnob("EQ_COLOR_B" + n, cell(4, 0));',
    'placeKnob("TAPE_DEGREE" + n, cell(4, 1));',
    'placeKnob("TRANSIENT" + n, cell(4, 2));',
]:
    assert token in cpp, token

# Exact main-strip visible labels/order
for token in [
    'label:"FREQ"',
    'label:"GAIN"',
    'label:"Q"',
    'label:"THRESH"',
    'label:"ATTACK"',
    'label:"RELEASE"',
    'label:"DEGREE"',
    'label:"LEVEL"',
    'label:"MIX"',
    'label:"AMOUNT"',
    'label:"TYPEA"',
    'label:"TRANSIENT"',
]:
    assert token in web, token

# 3-column Analog row and bottom buttons.
for token in [
    "grid-template-columns:repeat(3,1fr)!important",
    "top:423px!important",
    "top:516px!important",
    "MIX + ADV",
]:
    assert token in web, token

# MASTER is 8 px left of the equal fifth grid slot in the reference.
assert "const int x = masterRuntime ? 1004" in cpp
assert "transform:translateX(-8px)!important" in web

# Preserve the exact Ivory graph/frame rather than repainting a second background.
assert "metalLook.isMasterRuntimeActive() && !ivoryTheme" in cpp
assert "const exactIvory=masterRuntimeActive&&ivoryTheme;" in web
assert "if(!exactIvory){" in web

print("OKK(2) exact layout contract: PASS")
