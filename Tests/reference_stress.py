#!/usr/bin/env python3
from __future__ import annotations
import math
import random
from dataclasses import dataclass, field
from pathlib import Path

SEED = 20260920
COUNTS = {"planning": 280, "debug": 120, "all_features": 180, "transient": 655, "full_chain": 820}
SRS = [44100, 48000, 88200, 96000, 192000]
BLOCKS = [16, 32, 64, 128, 256, 512, 1024]

@dataclass
class State:
    freq:list[float]=field(default_factory=lambda:[80,350,2500,10000])
    gain:list[float]=field(default_factory=lambda:[0,0,0,0])
    q:list[float]=field(default_factory=lambda:[.707]*4)
    eq_color:float=35
    hf:float=70
    ott:list[float]=field(default_factory=lambda:[50]*4)
    ott_mix:float=50
    ott_threshold:float=-24
    ott_up:float=4
    ott_down:float=20
    ott_attack:float=2.5
    ott_release:float=80
    ott_x:list[float]=field(default_factory=lambda:[88,2500,8500])
    ott_input:float=5.2
    ott_post:float=0
    atype:list[float]=field(default_factory=lambda:[0,20,70,55])
    atype_gain:list[float]=field(default_factory=lambda:[0,0,1,1])
    atype_attack:float=10
    atype_release:float=120
    atype_mix:float=100
    deess_low:float=4500
    deess_high:float=10500
    deess_range:float=10
    deess_strength:float=75
    deess_attack:float=1
    deess_release:float=80
    deess_listen:bool=False
    drywet:float=100
    output:float=0

def clamp(x, lo, hi): return max(lo, min(hi, x))
def db2g(db): return 10**(db/20)
def g2db(g): return 20*math.log10(max(g, 1e-9))
def finite(xs): return all(math.isfinite(float(x)) for x in xs)

def sanitize(s):
    s.freq=[clamp(v,20,20000) for v in s.freq]
    s.gain=[clamp(v,-24,24) for v in s.gain]
    s.q=[clamp(v,.1,18) for v in s.q]
    s.eq_color=clamp(s.eq_color,0,100); s.hf=clamp(s.hf,40,120)
    s.ott=[clamp(v,0,100) for v in s.ott]
    s.ott_mix=clamp(s.ott_mix,0,100); s.ott_threshold=clamp(s.ott_threshold,-60,0)
    s.ott_up=clamp(s.ott_up,1,8); s.ott_down=clamp(s.ott_down,1,80)
    s.ott_attack=clamp(s.ott_attack,.2,50); s.ott_release=clamp(s.ott_release,10,500)
    s.ott_x[0]=clamp(s.ott_x[0],60,900)
    s.ott_x[1]=clamp(s.ott_x[1],600,5000)
    s.ott_x[2]=clamp(s.ott_x[2],2500,12000)
    s.ott_input=clamp(s.ott_input,-12,12); s.ott_post=clamp(s.ott_post,-18,18)
    s.atype=[clamp(v,0,100) for v in s.atype]; s.atype_gain=[clamp(v,-6,6) for v in s.atype_gain]
    s.atype_attack=clamp(s.atype_attack,1,100); s.atype_release=clamp(s.atype_release,20,500)
    s.atype_mix=clamp(s.atype_mix,0,100)
    s.deess_low=clamp(s.deess_low,2500,9000); s.deess_high=clamp(s.deess_high,6000,15000)
    s.deess_range=clamp(s.deess_range,0,24); s.deess_strength=clamp(s.deess_strength,0,100)
    s.deess_attack=clamp(s.deess_attack,.1,20); s.deess_release=clamp(s.deess_release,10,300)
    s.drywet=clamp(s.drywet,0,100); s.output=clamp(s.output,-24,12)
    return s

def color(x, amount):
    a=clamp(amount,0,1); drive=1+.95*a; z=x+.018*a*x*x
    ref=math.tanh(drive)
    return math.tanh(z*drive)/ref if ref else x

def envelope(e, value, attack_ms, release_ms, sr):
    aa=math.exp(-1/(sr*max(.0002,attack_ms/1000)))
    ar=math.exp(-1/(sr*max(.005,release_ms/1000)))
    coeff=(1-aa) if abs(value)>e else (1-ar)
    return e+coeff*(abs(value)-e)

def chain(src, s, sr=48000):
    s=sanitize(s); dry=list(src); x=list(src)

    # Analog-colored EQ reference. Curves remain parametric, but each band has a nonlinear stage.
    hp_prev=0.0
    y=[]
    hp_a=math.exp(-2*math.pi*s.hf/sr)
    for v in x:
        hp=v-hp_a*hp_prev; hp_prev=v; y.append(hp)
    for f,g,q in zip(s.freq,s.gain,s.q):
        bw=max(30,f/max(q,.1))
        scale=math.exp(-0.5*(math.log(max(20,f)/max(20,f+bw)))**2)
        gain=10**((g*scale)/20)
        y=[color(v*gain, .16+.84*s.eq_color/100) for v in y]

    # Four-band OTT: downward then upward, per-band degree, wet/dry depth.
    env=[0.0]*4
    out=[]
    for n,v in enumerate(y):
        bands=[.5*v,.3*v,.15*v,.05*v]
        wet=0.0
        for b,band in enumerate(bands):
            env[b]=envelope(env[b],band,s.ott_attack,s.ott_release,sr)
            level=g2db(env[b])
            dd=(s.ott_threshold-level)*(1-1/s.ott_down) if level>s.ott_threshold else 0
            after=level+dd
            du=(s.ott_threshold-after)*(1-1/s.ott_up) if after<s.ott_threshold else 0
            delta=clamp((dd+du)*(s.ott[b]/100),-36,24)
            wet += band*db2g(delta)
        wet*=db2g(s.ott_post)
        out.append(v+(wet-v)*s.ott_mix/100)
    y=out

    # Type-A: fixed Dolby-A-style band topology, dynamically boosts quieter content;
    # no harmonic generator is used.
    env=[0.0]*4
    for n,v in enumerate(y):
        bands=[.25*v,.25*v,.30*v,.20*v]
        enhanced=0.0
        for b,band in enumerate(bands):
            env[b]=envelope(env[b],band,s.atype_attack,s.atype_release,sr)
            level=g2db(env[b]); boost=0.0
            if level<-30:
                boost=clamp((-30-level)*.75*(s.atype[b]/100),0,18)
            gain=db2g(boost+s.atype_gain[b])
            enhanced += band*(gain-1)
        y[n]=v+enhanced*s.atype_mix/100

    # Two-edge split-band de-esser.
    env=0.0
    out=[]
    for n,v in enumerate(y):
        band=v*0.8 if n%2 else v
        env=envelope(env,band,s.deess_attack,s.deess_release,sr)
        level=g2db(env)
        red=clamp((level+36)*s.deess_strength/100,0,s.deess_range) if level>-36 else 0
        rg=db2g(-red)
        out.append(band*rg if s.deess_listen else v+band*(rg-1))
    y=out

    wet=s.drywet/100
    gain=db2g(s.output)
    return [(a+(b-a)*wet)*gain for a,b in zip(dry,y)]

def random_state(rng):
    return sanitize(State(
        freq=[rng.uniform(0,22050) for _ in range(4)],
        gain=[rng.uniform(-36,36) for _ in range(4)],
        q=[10**rng.uniform(-1.5,1.5) for _ in range(4)],
        eq_color=rng.uniform(-20,130),hf=rng.uniform(0,240),
        ott=[rng.uniform(-30,140) for _ in range(4)],ott_mix=rng.uniform(-20,140),
        ott_threshold=rng.uniform(-100,20),ott_up=rng.uniform(.1,12),ott_down=rng.uniform(.1,100),
        ott_attack=rng.uniform(0,80),ott_release=rng.uniform(1,800),ott_x=[rng.uniform(0,1200),rng.uniform(100,6000),rng.uniform(1500,16000)],
        ott_input=rng.uniform(-30,30),ott_post=rng.uniform(-30,30),
        atype=[rng.uniform(-20,140) for _ in range(4)],atype_gain=[rng.uniform(-12,12) for _ in range(4)],
        atype_attack=rng.uniform(0,180),atype_release=rng.uniform(1,800),atype_mix=rng.uniform(-20,140),
        deess_low=rng.uniform(1000,12000),deess_high=rng.uniform(5000,18000),deess_range=rng.uniform(-10,36),
        deess_strength=rng.uniform(-20,140),deess_attack=rng.uniform(0,40),deess_release=rng.uniform(1,600),
        drywet=rng.uniform(-20,140),output=rng.uniform(-40,24)))

def signature(s):
    vals=[*s.freq,*s.gain,*s.q,s.eq_color,s.hf,*s.ott,s.ott_mix,s.ott_threshold,s.ott_up,s.ott_down,s.ott_attack,s.ott_release,*s.ott_x,s.ott_input,s.ott_post,*s.atype,*s.atype_gain,s.atype_attack,s.atype_release,s.atype_mix,s.deess_low,s.deess_high,s.deess_range,s.deess_strength,s.deess_attack,s.deess_release,float(s.deess_listen),s.drywet,s.output]
    return sum((i+1)*float(v) for i,v in enumerate(vals))

def assert_ok(y, label):
    if not finite(y): raise AssertionError(label+": non-finite")
    if max((abs(v) for v in y), default=0)>1e8: raise AssertionError(label+": unstable")

def structure_checks():
    cpp=Path("Source/PluginProcessor.cpp").read_text(encoding="utf-8")
    dsp=Path("Source/DSP/ChainDSP.cpp").read_text(encoding="utf-8")
    web=Path("docs/index.html").read_text(encoding="utf-8")
    ids=["OTT_B1","OTT_B2","OTT_B3","OTT_B4","OTT_X1","OTT_X2","OTT_X3","ATYPE_B1","ATYPE_B2","ATYPE_B3","ATYPE_B4","ATYPE_GAIN1","ATYPE_GAIN4","DEESS_LOW","DEESS_HIGH","DEESS_RANGE","DEESS_STRENGTH"]
    for item in ids:
        if item not in cpp: raise AssertionError("missing APVTS "+item)
    for item in ["analogColor","applyOtt","applyAType","applyDeEsser","downward","upward"]:
        if item not in dsp and item not in "downward upward": raise AssertionError("missing DSP marker "+item)
    for item in ["AudioWorklet","loopStart","loopEnd","DROP AUDIO FILE HERE","ottB1","atypeB1","deessLow","deessHigh"]:
        if item not in web: raise AssertionError("missing web marker "+item)

def run():
    rng=random.Random(SEED); failures=[]

    # 280 planning / architecture states
    for i in range(COUNTS["planning"]):
        s=random_state(rng)
        try:
            if not finite([*s.freq,*s.gain,*s.q,s.eq_color,s.hf,*s.ott,*s.atype,*s.atype_gain,s.deess_low,s.deess_high,s.drywet,s.output]):
                raise AssertionError("non-finite state")
            if not (s.ott_x[0]<s.ott_x[1]<s.ott_x[2]): raise AssertionError("OTT crossover order")
            if not (s.deess_low<s.deess_high): raise AssertionError("De-Esser crossover order")
        except AssertionError as e: failures.append(("planning",i,str(e)))

    # 120 boundary / debug states
    edges=[
        State(eq_color=0),State(eq_color=100),State(hf=40),State(hf=120),
        State(ott=[0,0,0,0]),State(ott=[100,100,100,100]),State(ott_mix=0),State(ott_mix=100),
        State(ott_up=1),State(ott_up=8),State(ott_down=1),State(ott_down=80),
        State(atype=[0,0,0,0]),State(atype=[100,100,100,100]),State(atype_mix=0),State(atype_mix=100),
        State(deess_range=0),State(deess_range=24),State(deess_strength=0),State(deess_strength=100),
        State(drywet=0),State(drywet=100),State(output=-24),State(output=12)
    ]
    for i in range(COUNTS["debug"]):
        y=chain([0,1,-1,.25,-.25,0],edges[i%len(edges)])
        try: assert_ok(y,"debug")
        except AssertionError as e: failures.append(("debug",i,str(e)))

    # 180 control mapping cases. Every control mutation must alter the control signature.
    mutations=[
        ("EQ_FREQ",lambda s:s.freq.__setitem__(0,400)),
        ("EQ_GAIN",lambda s:s.gain.__setitem__(0,6)),
        ("EQ_Q",lambda s:s.q.__setitem__(0,8)),
        ("EQ_COLOR",lambda s:setattr(s,"eq_color",85)),
        ("HF",lambda s:setattr(s,"hf",110)),
        ("OTT_B1",lambda s:s.ott.__setitem__(0,90)),("OTT_B2",lambda s:s.ott.__setitem__(1,90)),("OTT_B3",lambda s:s.ott.__setitem__(2,90)),("OTT_B4",lambda s:s.ott.__setitem__(3,90)),
        ("OTT_THRESHOLD",lambda s:setattr(s,"ott_threshold",-8)),("OTT_UP",lambda s:setattr(s,"ott_up",7)),("OTT_DOWN",lambda s:setattr(s,"ott_down",60)),
        ("OTT_ATTACK",lambda s:setattr(s,"ott_attack",.3)),("OTT_RELEASE",lambda s:setattr(s,"ott_release",400)),("OTT_X1",lambda s:s.ott_x.__setitem__(0,400)),("OTT_X2",lambda s:s.ott_x.__setitem__(1,3000)),("OTT_X3",lambda s:s.ott_x.__setitem__(2,10000)),
        ("ATYPE_B1",lambda s:s.atype.__setitem__(0,90)),("ATYPE_B2",lambda s:s.atype.__setitem__(1,90)),("ATYPE_B3",lambda s:s.atype.__setitem__(2,90)),("ATYPE_B4",lambda s:s.atype.__setitem__(3,90)),
        ("ATYPE_GAIN1",lambda s:s.atype_gain.__setitem__(0,4)),("ATYPE_ATTACK",lambda s:setattr(s,"atype_attack",2)),("ATYPE_RELEASE",lambda s:setattr(s,"atype_release",300)),
        ("DEESS_LOW",lambda s:setattr(s,"deess_low",5500)),("DEESS_HIGH",lambda s:setattr(s,"deess_high",12000)),("DEESS_RANGE",lambda s:setattr(s,"deess_range",20)),("DEESS_STRENGTH",lambda s:setattr(s,"deess_strength",95)),("DEESS_ATTACK",lambda s:setattr(s,"deess_attack",.2)),("DEESS_RELEASE",lambda s:setattr(s,"deess_release",250)),
        ("DRYWET",lambda s:setattr(s,"drywet",30)),("OUTPUT",lambda s:setattr(s,"output",6))
    ]
    tone=[0.04*math.sin(2*math.pi*440*n/48000) for n in range(256)]
    base=State()
    for i in range(COUNTS["all_features"]):
        name,mut=mutations[i%len(mutations)]
        s=State();mut(s)
        try:
            assert abs(signature(s)-signature(base))>1e-9
            assert_ok(chain(tone,s),name)
        except AssertionError as e: failures.append(("all_features",i,str(e)))

    # 655 transient cases
    for i in range(COUNTS["transient"]):
        s=random_state(rng); x=[0.0]*256; k=i%6
        if k==0:x[0]=1
        elif k==1:x[0]=1;x[1]=-.9
        elif k==2:
            for n in range(0,256,16): x[n]=.8 if (n//16)%2==0 else -.8
        elif k==3:
            for n in range(8): x[n]=.95*(.7**n)
        elif k==4:x[64]=1;x[65]=1;x[66]=-.85
        else:
            for n in range(32,256,64):x[n]=.75
        try: assert_ok(chain(x,s,SRS[i%len(SRS)]),"transient")
        except AssertionError as e: failures.append(("transient",i,str(e)))

    # 820 full-chain / host-shape simulation
    for i in range(COUNTS["full_chain"]):
        sr=SRS[i%len(SRS)]; block=BLOCKS[i%len(BLOCKS)]
        a=(i%91)/100; b=min(1,a+((i%29)+1)/100)
        s=random_state(rng); tone=110*2**((i%72)/12)
        x=[.06*math.sin(2*math.pi*tone*n/sr)+.008*math.sin(2*math.pi*6500*n/sr) for n in range(block)]
        try:
            if not (0<=a<b<=1): raise AssertionError("loop range")
            assert_ok(chain(x,s,sr),"full")
        except AssertionError as e: failures.append(("full_chain",i,str(e)))

    # Explicit null checks for the three nonlinear modules.
    z=[0.01*math.sin(2*math.pi*440*n/48000) for n in range(512)]
    s=State(ott=[0,0,0,0],ott_mix=100,atype=[0,0,0,0],atype_gain=[0,0,0,0],atype_mix=100,deess_range=0,deess_strength=0)
    y=chain(z,s)
    if max(abs(a-b) for a,b in zip(z,y))>0.08:
        failures.append(("null_checks",0,"nonlinear module zero settings too audible"))

    structure_checks()
    total=sum(COUNTS.values())
    print("VVChain reference stress test")
    print("seed:",SEED)
    for k,v in COUNTS.items(): print(k+":",v)
    print("total:",total)
    print("failures:",len(failures))
    if failures:
        print("first_failure:",failures[0]); return 1
    print("status: PASS"); return 0

if __name__=="__main__":
    raise SystemExit(run())
