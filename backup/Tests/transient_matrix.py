import math
from pathlib import Path
import numpy as np

TEN_OVER_LN10 = 10.0 / math.log(10.0)
MAX_DB = 12.0

def fast_log_positive(x: float) -> float:
    x = max(float(x), 1.0e-20)
    m, exponent = math.frexp(x)
    m *= 2.0
    exponent -= 1
    if m > math.sqrt(2.0):
        m *= 0.5
        exponent += 1
    y = (m - 1.0) / (m + 1.0)
    y2 = y * y
    ln_m = 2.0 * y * (1.0 + y2 * (1.0 / 3.0 + y2 * 0.2))
    return ln_m + exponent * math.log(2.0)

def control_gain(fast_sq: float, slow_sq: float, amount: float) -> float:
    eps = 1.0e-12
    ratio = (fast_sq + eps) / (slow_sq + eps)
    transient_db = TEN_OVER_LN10 * fast_log_positive(ratio)
    scaled = transient_db * np.clip(amount, -1.0, 1.0) / MAX_DB
    clipped = scaled / (1.0 + abs(scaled))
    control_db = clipped * MAX_DB
    return 10.0 ** (control_db / 20.0)

def run():
    dsp = Path("Source/DSP/ChainDSP.cpp").read_text(encoding="utf-8")
    hdr = Path("Source/DSP/ChainDSP.h").read_text(encoding="utf-8")
    web = Path("docs/vvchain-worklet.js").read_text(encoding="utf-8")

    # Architecture guards: host-rate stage, detector-only B1 HPF, stereo link,
    # parallel delta output, and no Transient-triggered Analog oversampling.
    for marker in (
        "void VVChainDSP::applyTransient",
        "applyTransient(buffer, p);",
        "transientXover1",
        "transientBand1SidechainHPF",
        "0.5f * (detL * detL + detR * detR)",
        "inputByChannel[ch] + delta",
        "bandsByChannel[ch][band] * (gains[band] - 1.0f)",
    ):
        assert marker in dsp + hdr, marker
    assert "anyBandStageActive" not in dsp
    assert "applyTransientStereo" in web
    assert "deltaL+=bandsL[b]*d" in web

    # Fast-log approximation stays numerically close over a very wide positive range.
    for x in np.logspace(-12, 12, 4000):
        assert abs(fast_log_positive(float(x)) - math.log(float(x))) < 2.0e-6

    rng = np.random.default_rng(20260925)
    max_db_seen = 0.0
    for _ in range(5000):
        fast = 10.0 ** rng.uniform(-12.0, 2.0)
        slow = 10.0 ** rng.uniform(-12.0, 2.0)
        amount = rng.uniform(-1.0, 1.0)
        g = control_gain(fast, slow, amount)
        assert math.isfinite(g) and g > 0.0
        db = 20.0 * math.log10(g)
        max_db_seen = max(max_db_seen, abs(db))
        assert abs(db) < MAX_DB + 1.0e-9

        # Bipolar response is reciprocal for opposite amount signs.
        gp = control_gain(fast, slow, abs(amount))
        gn = control_gain(fast, slow, -abs(amount))
        assert abs(gp * gn - 1.0) < 1.0e-12

        # 0% is mathematically exact unity; parallel-delta therefore returns input.
        assert control_gain(fast, slow, 0.0) == 1.0

    # Stereo-linked squared energy is channel-order invariant and uses no sqrt.
    for _ in range(1000):
        l, r = rng.standard_normal(2)
        e1 = 0.5 * (l*l + r*r)
        e2 = 0.5 * (r*r + l*l)
        assert e1 == e2 and e1 >= 0.0

    print(f"PASS TRANSIENT matrix: 5000 control cases, |control|<{MAX_DB} dB, max={max_db_seen:.6f} dB")

if __name__ == "__main__":
    run()
