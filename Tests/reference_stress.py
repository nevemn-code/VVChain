#!/usr/bin/env python3
from __future__ import annotations
import math, random
from dataclasses import dataclass, field
from pathlib import Path

SEED = 20260920
COUNTS = {
    "planning": 280,
    "debug": 120,
    "all_features": 180,
    "transient": 655,
    "full_chain": 820,
}
SRS = [44100, 48000, 88200, 96000, 192000]
BLOCKS = [16, 32, 64, 128, 256, 512, 1024]

@dataclass
class State:
    freq:list[float]=field(default_factory=lambda:[80.0,350.0,2500.0,10000.0])
    gain:list[float]=field(default_factory=lambda:[0.0]*4)
    q:list[float]=field(default_factory=lambda:[0.707]*4)
    hf:float=70.0

    ott_depth:float=50.0
    ott_mix:float=50.0
    ott_threshold:float=-16.0
    ott_up:float=2.0
    ott_down:float=4.0
    ott_attack:float=2.5
    ott_release:float=80.0
    ott_x1:float=180.0
    ott_x2:float=1400.0
    ott_x3:float=6500.0
    ott_post:float=0.0

    atype_amount:float=20.0
    atype_drive:float=6.0
    atype_bias:float=0.0
    atype_mix:float=100.0
    atype_tone:float=70.0
    atype_hpf:float=60.0

    deess_freq:float=6500.0
    deess_q:float=1.2
    deess_threshold:float=-30.0
    deess_range:float=8.0
    deess_attack:float=1.0
    deess_release:float=80.0
    deess_listen:bool=False

    drywet:float=100.0
    output:float=0.0

def clamp(x, lo, hi):
    return max(lo, min(hi, x))

def sanitize(s: State):
    s.freq=[clamp(v,20,20000) for v in s.freq]
    s.gain=[clamp(v,-24,24) for v in s.gain]
    s.q=[clamp(v,.1,18) for v in s.q]
    s.hf=clamp(s.hf,40,120)
    s.ott_depth=clamp(s.ott_depth,0,100)
    s.ott_mix=clamp(s.ott_mix,0,100)
    s.ott_threshold=clamp(s.ott_threshold,-60,0)
    s.ott_up=clamp(s.ott_up,1,8)
    s.ott_down=clamp(s.ott_down,1,20)
    s.ott_attack=clamp(s.ott_attack,.1,30)
    s.ott_release=clamp(s.ott_release,10,500)
    s.ott_x1=clamp(s.ott_x1,60,900)
    s.ott_x2=clamp(s.ott_x2,600,3500)
    s.ott_x3=clamp(s.ott_x3,2500,12000)
    s.atype_amount=clamp(s.atype_amount,0,100)
    s.atype_drive=clamp(s.atype_drive,0,24)
    s.atype_bias=clamp(s.atype_bias,-100,100)
    s.atype_mix=clamp(s.atype_mix,0,100)
    s.atype_tone=clamp(s.atype_tone,0,100)
    s.atype_hpf=clamp(s.atype_hpf,20,1000)
    s.deess_freq=clamp(s.deess_freq,2000,12000)
    s.deess_q=clamp(s.deess_q,.2,10)
    s.deess_threshold=clamp(s.deess_threshold,-80,0)
    s.deess_range=clamp(s.deess_range,0,24)
    s.deess_attack=clamp(s.deess_attack,.1,20)
    s.deess_release=clamp(s.deess_release,10,300)
    s.drywet=clamp(s.drywet,0,100)
    s.output=clamp(s.output,-24,12)
    return s

def finite(values):
    return all(math.isfinite(float(v)) for v in values)

def tone_signal(sr, n):
    tones=(80,350,1000,2500,6500,10000,14000)
    return [
        sum(0.015*math.sin(2*math.pi*f*i/sr) for f in tones)
        for i in range(n)
    ]

def envelope(x, attack, release, sr):
    aA=math.exp(-1/(sr*max(.0001,attack)))
    aR=math.exp(-1/(sr*max(.005,release)))
    env=0.0
    out=[]
    for v in x:
        av=abs(v)
        c=(1-aA) if av>env else (1-aR)
        env += c*(av-env)
        out.append(env)
    return out

def process(src, s: State, sr=48000):
    s=sanitize(s)
    x=list(src)
    dry=list(x)

    # EQ + HPF reference
    hp_a=math.exp(-2*math.pi*s.hf/sr)
    hp_prev=0.0
    y=[]
    for v in x:
        hp=v-hp_a*hp_prev
        hp_prev=v
        y.append(hp)

    for f,g,q in zip(s.freq,s.gain,s.q):
        bw=max(30.0, f/max(q,.1))
        influence=[]
        for n,v in enumerate(y):
            local = abs(math.sin(2*math.pi*f*n/sr))
            # stable, smooth proxy for a peak filter response
            influence.append(10**((g * math.exp(-0.5*((math.log(max(20,f)/max(20,f+max(bw,1))))**2))) / 20))
        y=[v*gain for v,gain in zip(y,influence)]

    # 4-band OTT-style reference
    env=envelope(y,s.ott_attack/1000,s.ott_release/1000,sr)
    ott=[]
    depth=s.ott_depth/100
    mix=s.ott_mix/100
    for v,e in zip(y,env):
        level=20*math.log10(max(e,1e-9))
        if level>s.ott_threshold:
            delta=(s.ott_threshold-level)*(1-1/max(s.ott_down,1))
        else:
            delta=(s.ott_threshold-level)*(1-1/max(s.ott_up,1))
        delta=clamp(delta*depth,-24,18)
        wet=v*10**(delta/20)
        ott.append(v+mix*(wet-v))
    post=10**(s.ott_post/20)
    ott=[v*post for v in ott]

    # A-Type reference
    amount=s.atype_amount/100
    drive=10**(s.atype_drive/20)
    bias=s.atype_bias/100*.02
    mixA=s.atype_mix/100
    gainA=[]
    for v in ott:
        z=v*drive+bias
        shaped=math.tanh(z*(1+2.8*amount))
        gainA.append(v+mixA*amount*1.25*shaped)

    # Split-band de-esser reference
    de_env=envelope(gainA,s.deess_attack/1000,s.deess_release/1000,sr)
    de=[]
    for i,v in enumerate(gainA):
        band=v*math.sin(2*math.pi*s.deess_freq*(i+1)/sr)
        lvl=20*math.log10(max(abs(band),1e-9))
        red=-clamp(lvl-s.deess_threshold,0,s.deess_range) if lvl>s.deess_threshold else 0
        rg=10**(red/20)
        if s.deess_listen:
            de.append(band*rg)
        else:
            de.append(v+band*(rg-1))
    wet=s.drywet/100
    out=10**(s.output/20)
    return [(d+wet*(v-d))*out for d,v in zip(dry,de)]

def random_state(rng):
    return sanitize(State(
        freq=[rng.uniform(0,22050) for _ in range(4)],
        gain=[rng.uniform(-36,36) for _ in range(4)],
        q=[10**rng.uniform(-1.5,1.5) for _ in range(4)],
        hf=rng.uniform(0,240),
        ott_depth=rng.uniform(-30,140),ott_mix=rng.uniform(-30,140),
        ott_threshold=rng.uniform(-100,20),ott_up=rng.uniform(.1,12),
        ott_down=rng.uniform(.1,30),ott_attack=rng.uniform(0,60),
        ott_release=rng.uniform(1,800),ott_x1=rng.uniform(0,1200),
        ott_x2=rng.uniform(100,5000),ott_x3=rng.uniform(1500,16000),
        ott_post=rng.uniform(-30,30),
        atype_amount=rng.uniform(-20,140),atype_drive=rng.uniform(-10,36),
        atype_bias=rng.uniform(-140,140),atype_mix=rng.uniform(-20,140),
        atype_tone=rng.uniform(-20,140),atype_hpf=rng.uniform(0,1600),
        deess_freq=rng.uniform(1000,16000),deess_q=rng.uniform(.05,15),
        deess_threshold=rng.uniform(-120,20),deess_range=rng.uniform(-10,36),
        deess_attack=rng.uniform(0,40),deess_release=rng.uniform(1,600),
        drywet=rng.uniform(-20,140),output=rng.uniform(-40,24),
    ))


def control_signature(s: State):
    values = [
        *s.freq, *s.gain, *s.q, s.hf,
        s.ott_depth,s.ott_mix,s.ott_threshold,s.ott_up,s.ott_down,
        s.ott_attack,s.ott_release,s.ott_x1,s.ott_x2,s.ott_x3,s.ott_post,
        s.atype_amount,s.atype_drive,s.atype_bias,s.atype_mix,s.atype_tone,s.atype_hpf,
        s.deess_freq,s.deess_q,s.deess_threshold,s.deess_range,s.deess_attack,s.deess_release,
        float(s.deess_listen),s.drywet,s.output
    ]
    return sum((i+1)*float(v) for i,v in enumerate(values))

def rms(x):
    return math.sqrt(sum(v*v for v in x)/max(1,len(x)))

def assert_finite(x, label):
    if not finite(x):
        raise AssertionError(label + ": non-finite output")
    if max((abs(v) for v in x), default=0) > 1e8:
        raise AssertionError(label + ": unstable output")

def test_loop_math():
    for a,b in ((0,1),(.1,.2),(.65,.91),(.99,1)):
        assert 0<=a<b<=1
        assert (b-a)>0

def test_web_surface():
    html=Path("docs/index.html").read_text(encoding="utf-8")
    required=(
        "LOAD AUDIO","DROP AUDIO FILE HERE","selectionStart","selectionEnd",
        "source.loop","loopStart","loopEnd","BAND 1","ottThreshold","atypeDrive",
        "deessListen","requestAnimationFrame"
    )
    missing=[x for x in required if x not in html]
    if missing:
        raise AssertionError("web preview missing: "+", ".join(missing))

def run():
    rng=random.Random(SEED)
    failures=[]

    # 280: planning / parameter-space validation
    for i in range(COUNTS["planning"]):
        s=random_state(rng)
        vals=s.freq+s.gain+s.q+[s.hf,s.ott_depth,s.ott_mix,s.ott_threshold,s.ott_up,s.ott_down,
            s.ott_attack,s.ott_release,s.ott_x1,s.ott_x2,s.ott_x3,s.ott_post,s.atype_amount,
            s.atype_drive,s.atype_bias,s.atype_mix,s.atype_tone,s.atype_hpf,s.deess_freq,
            s.deess_q,s.deess_threshold,s.deess_range,s.deess_attack,s.deess_release,s.drywet,s.output]
        if not finite(vals): failures.append(("planning",i))
        if not (40<=s.hf<=120 and 0<=s.ott_depth<=100 and .1<=s.ott_attack<=30): failures.append(("planning-range",i))
    # 120: boundary / debug
    edges=[
        State(hf=40),State(hf=120),State(gain=[-24]*4),State(gain=[24]*4),
        State(q=[.1]*4),State(q=[18]*4),State(ott_depth=0),State(ott_depth=100),
        State(ott_up=1),State(ott_up=8),State(ott_down=1),State(ott_down=20),
        State(atype_amount=0),State(atype_amount=100),State(deess_range=0),State(deess_range=24),
        State(drywet=0),State(drywet=100),State(output=-24),State(output=12),
    ]
    for i in range(COUNTS["debug"]):
        y=process([0,1,-1,.25,-.25,0.0],edges[i%len(edges)],48000)
        try: assert_finite(y,f"debug {i}")
        except AssertionError as e: failures.append(("debug",i,str(e)))

    # 180: all-feature parameter sensitivity
    base=State()
    sig=tone_signal(48000,256)
    mutations=[
        ("EQ_FREQ",lambda s:setattr(s,"freq",[300,350,2500,10000])),
        ("EQ_GAIN",lambda s:setattr(s,"gain",[6,0,0,0])),
        ("EQ_Q",lambda s:setattr(s,"q",[8,.707,.707,.707])),
        ("HF",lambda s:setattr(s,"hf",110)),
        ("OTT_DEPTH",lambda s:setattr(s,"ott_depth",95)),
        ("OTT_MIX",lambda s:setattr(s,"ott_mix",90)),
        ("OTT_THRESHOLD",lambda s:setattr(s,"ott_threshold",-6)),
        ("OTT_UP",lambda s:setattr(s,"ott_up",7)),
        ("OTT_DOWN",lambda s:setattr(s,"ott_down",18)),
        ("OTT_ATTACK",lambda s:setattr(s,"ott_attack",.3)),
        ("OTT_RELEASE",lambda s:setattr(s,"ott_release",400)),
        ("OTT_X1",lambda s:setattr(s,"ott_x1",400)),
        ("OTT_X2",lambda s:setattr(s,"ott_x2",2500)),
        ("OTT_X3",lambda s:setattr(s,"ott_x3",10000)),
        ("OTT_POST",lambda s:setattr(s,"ott_post",6)),
        ("ATYPE_AMOUNT",lambda s:setattr(s,"atype_amount",90)),
        ("ATYPE_DRIVE",lambda s:setattr(s,"atype_drive",20)),
        ("ATYPE_BIAS",lambda s:setattr(s,"atype_bias",80)),
        ("ATYPE_MIX",lambda s:setattr(s,"atype_mix",30)),
        ("ATYPE_TONE",lambda s:setattr(s,"atype_tone",15)),
        ("ATYPE_HPF",lambda s:setattr(s,"atype_hpf",800)),
        ("DEESS_FREQ",lambda s:setattr(s,"deess_freq",8000)),
        ("DEESS_Q",lambda s:setattr(s,"deess_q",6)),
        ("DEESS_THRESHOLD",lambda s:setattr(s,"deess_threshold",-10)),
        ("DEESS_RANGE",lambda s:setattr(s,"deess_range",20)),
        ("DEESS_ATTACK",lambda s:setattr(s,"deess_attack",.2)),
        ("DEESS_RELEASE",lambda s:setattr(s,"deess_release",250)),
        ("DEESS_LISTEN",lambda s:setattr(s,"deess_listen",True)),
        ("DRYWET",lambda s:setattr(s,"drywet",30)),
        ("OUTPUT",lambda s:setattr(s,"output",6)),
    ]
    for i in range(COUNTS["all_features"]):
        name,mut=mutations[i%len(mutations)]
        s=State();mut(s);a=process(sig,s);b=process(sig,base)
        try:
            assert_finite(a,name)
            if rms([x-y for x,y in zip(a,b)]) < 1e-7:
                raise AssertionError(name+" has no observable effect")
        except AssertionError as e: failures.append(("all_features",i,str(e)))

    # 655: transient stress
    for i in range(COUNTS["transient"]):
        s=random_state(rng)
        n=256
        x=[0.0]*n
        typ=i%6
        if typ==0:x[0]=1.0
        elif typ==1:x[0]=1.0;x[1]=-.85
        elif typ==2:
            for k in range(0,n,16):x[k]=.8 if (k//16)%2==0 else -.8
        elif typ==3:
            for k in range(8):x[k]=0.95*(0.7**k)
        elif typ==4:
            x[64]=1;x[65]=1;x[66]=-.9
        else:
            for k in range(32,n,64):x[k]=.75
        y=process(x,s,SRS[i%len(SRS)])
        try:assert_finite(y,f"transient {i}")
        except AssertionError as e:failures.append(("transient",i,str(e)))

    # 820: full-chain simulation including sample-rate/block-size/loop ranges
    for i in range(COUNTS["full_chain"]):
        sr=SRS[i%len(SRS)]
        block=BLOCKS[i%len(BLOCKS)]
        a=(i%97)/100
        b=min(1.0,a+((i%31)+1)/100)
        if not (0<=a<b<=1): failures.append(("loop",i))
        s=random_state(rng)
        tone=110*2**((i%72)/12)
        x=[.06*math.sin(2*math.pi*tone*n/sr)+.008*math.sin(2*math.pi*6500*n/sr) for n in range(block)]
        y=process(x,s,sr)
        try:assert_finite(y,f"full {i}")
        except AssertionError as e:failures.append(("full_chain",i,str(e)))

    test_loop_math()
    test_web_surface()

    total=sum(COUNTS.values())
    print("VVChain reference stress test")
    print("Seed:",SEED)
    for k,v in COUNTS.items(): print(f"{k}: {v}")
    print("total:",total)
    print("failures:",len(failures))
    if failures:
        print("first_failure:",failures[0])
        return 1
    print("status: PASS")
    return 0

if __name__=="__main__":
    raise SystemExit(run())
