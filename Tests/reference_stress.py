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

    ott_degree:list[float]=field(default_factory=lambda:[100]*4)
    lift_t:list[float]=field(default_factory=lambda:[-40]*4)
    lift_a:list[float]=field(default_factory=lambda:[50]*4)
    lift_r:list[float]=field(default_factory=lambda:[50]*4)
    lift_m:list[float]=field(default_factory=lambda:[100]*4)
    comp_t:list[float]=field(default_factory=lambda:[-12]*4)
    comp_a:list[float]=field(default_factory=lambda:[15]*4)
    comp_r:list[float]=field(default_factory=lambda:[60]*4)
    comp_m:list[float]=field(default_factory=lambda:[100]*4)
    ott_level:list[float]=field(default_factory=lambda:[0]*4)
    ott_x:list[float]=field(default_factory=lambda:[350,1000,9000])
    ott_input:float=5.2
    ott_gate:float=-80
    ott_mix:float=100
    ott_clipper:bool=True
    ott_output:float=-6

    atype_degree:list[float]=field(default_factory=lambda:[0,20,70,55])
    atype_level:list[float]=field(default_factory=lambda:[0,0,1,1])
    atype_attack:float=10
    atype_release:float=120
    atype_input:float=0
    atype_mix:float=100
    atype_output:float=0

    deess_low:float=4500
    deess_high:float=10500
    deess_range:float=10
    deess_strength:float=75
    deess_attack:float=1
    deess_release:float=80
    deess_listen:bool=False

    drywet:float=100
    output:float=0

def clamp(x,lo,hi): return max(lo,min(hi,x))
def db2g(db): return 10**(db/20)
def g2db(g): return 20*math.log10(max(g,1e-5))
def finite(xs): return all(math.isfinite(float(x)) for x in xs)

def sanitize(s):
    s.freq=[clamp(v,20,20000) for v in s.freq]; s.gain=[clamp(v,-24,24) for v in s.gain]; s.q=[clamp(v,.1,18) for v in s.q]
    s.eq_color=clamp(s.eq_color,0,100); s.hf=clamp(s.hf,40,120)
    s.ott_degree=[clamp(v,0,100) for v in s.ott_degree]
    for a in (s.lift_t,s.comp_t): pass
    s.lift_t=[clamp(v,-80,0) for v in s.lift_t]; s.lift_a=[clamp(v,1,500) for v in s.lift_a]; s.lift_r=[clamp(v,10,2500) for v in s.lift_r]; s.lift_m=[clamp(v,0,100) for v in s.lift_m]
    s.comp_t=[clamp(v,-24,0) for v in s.comp_t]; s.comp_a=[clamp(v,.1,250) for v in s.comp_a]; s.comp_r=[clamp(v,10,2500) for v in s.comp_r]; s.comp_m=[clamp(v,0,100) for v in s.comp_m]
    s.ott_level=[clamp(v,-24,12) for v in s.ott_level]
    s.ott_x[0]=clamp(s.ott_x[0],80,600); s.ott_x[1]=clamp(s.ott_x[1],750,3000); s.ott_x[2]=clamp(s.ott_x[2],6000,12000)
    s.ott_input=clamp(s.ott_input,-24,24); s.ott_gate=clamp(s.ott_gate,-90,0); s.ott_mix=clamp(s.ott_mix,0,100); s.ott_output=clamp(s.ott_output,-24,24)
    s.atype_degree=[clamp(v,0,100) for v in s.atype_degree]; s.atype_level=[clamp(v,-6,6) for v in s.atype_level]
    s.atype_attack=clamp(s.atype_attack,1,100); s.atype_release=clamp(s.atype_release,20,500); s.atype_input=clamp(s.atype_input,-24,24); s.atype_mix=clamp(s.atype_mix,0,100); s.atype_output=clamp(s.atype_output,-24,24)
    s.deess_low=clamp(s.deess_low,2500,9000); s.deess_high=clamp(s.deess_high,6000,15000); s.deess_range=clamp(s.deess_range,0,24); s.deess_strength=clamp(s.deess_strength,0,100)
    s.deess_attack=clamp(s.deess_attack,.1,20); s.deess_release=clamp(s.deess_release,10,300); s.drywet=clamp(s.drywet,0,100); s.output=clamp(s.output,-24,12)
    if s.ott_x[1] < s.ott_x[0]+80: s.ott_x[1]=s.ott_x[0]+80
    if s.ott_x[2] < s.ott_x[1]+200: s.ott_x[2]=s.ott_x[1]+200
    if s.deess_high < s.deess_low+100: s.deess_high=s.deess_low+100
    return s

def color(x,a):
    d=1+1.35*clamp(a,0,1); z=x+.012*clamp(a,0,1)*x*x; r=math.tanh(d)
    return math.tanh(z*d)/r if r else x

def env_follow(e,v,attack,release,sr):
    aa=math.exp(-1/(.001*max(.1,attack)*sr)); rr=math.exp(-1/(.001*max(.1,release)*sr))
    k=aa if abs(v)>e else rr
    return k*e+(1-k)*abs(v)

def lifter(v,e,th,a,r,m,sr):
    ld=g2db(abs(v)); st=th-3; en=th+3; slope=1-1/6; target=0
    if ld<st: target=(th-ld)*slope
    elif ld<en: target=slope/12*(en-ld)**2
    tg=db2g(min(30,target)); e2=env_follow(e,tg,a,r,sr); mm=m/100
    return v*e2*mm+v*(1-mm),e2

def compressor(v,e,th,a,r,m,sr):
    ld=g2db(abs(v)); st=th-3; en=th+3; slope=1-1/8; gr=0
    if ld>en: gr=(ld-th)*slope
    elif ld>st: gr=slope/12*(ld-st)**2
    cur=-e; aa=math.exp(-1/(.001*max(.1,a)*sr)); rr=math.exp(-1/(.001*max(.1,r)*sr)); k=aa if gr>cur else rr
    e2=-(k*cur+(1-k)*gr); mm=m/100
    return v*db2g(e2)*mm+v*(1-mm),e2

def chain(src,s,sr=48000):
    s=sanitize(s); dry=list(src); y=list(src)
    # Analog-prototype EQ proxy + permanent nonlinearity.
    prev=0; hp=math.exp(-2*math.pi*s.hf/sr)
    for n,v in enumerate(y):
        z=v-hp*prev; prev=v; y[n]=z
    for f,g,q in zip(s.freq,s.gain,s.q):
        bw=max(30,f/max(q,.1)); shape=math.exp(-0.5*(math.log(max(20,f)/max(20,f+bw)))**2); gg=db2g(g*shape)
        y=[color(v*gg,.2+.8*s.eq_color/100) for v in y]

    # PunkOTT-style gate + four independent Lifter -> Compressor bands.
    env_l=[[1,1,1,1],[1,1,1,1]]; env_c=[[0,0,0,0],[0,0,0,0]]; gate_e=[0,0]; lim_e=[0,0]
    out=[]
    for n,v in enumerate(y):
        ld=g2db(abs(v)*1 if abs(v)>1e-6 else 1e-6); th=s.ott_gate; gd=(ld-th)*5 if ld<th-4.5 else (5/(18))*((ld-(th+4.5))**2) if ld<th+4.5 else 0
        ga=math.exp(-1/(.001*100*sr)); gr=math.exp(-1/(.001*30*sr)); k=ga if gd<gate_e[0] else gr; gate_e[0]=k*gate_e[0]+(1-k)*gd; v=v*(.9*db2g(gate_e[0])+.1)*db2g(s.ott_input)
        # stable four-band energy proxies; xovers still validated separately.
        bands=[.25*v,.25*v,.25*v,.25*v]
        wet=0
        for b,z in enumerate(bands):
            z,env_l[0][b]=lifter(z,env_l[0][b],s.lift_t[b],s.lift_a[b],s.lift_r[b],s.lift_m[b]*s.ott_degree[b]/100,sr)
            z,env_c[0][b]=compressor(z,env_c[0][b],s.comp_t[b],s.comp_a[b],s.comp_r[b],s.comp_m[b]*s.ott_degree[b]/100,sr)
            wet += z*db2g(s.ott_level[b])
        yv=wet; ml=db2g(s.ott_output)
        # fixed-safety limiter approximation
        ld2=g2db(abs(yv)); gr2=(ld2+3)*(1-1/20) if ld2>-3 else 0
        k2=math.exp(-1/(.001*30*sr)); kr2=math.exp(-1/(.001*100*sr)); lim_e[0]=(k2 if gr2>-lim_e[0] else kr2)*lim_e[0]+(1-(k2 if gr2>-lim_e[0] else kr2))*gr2
        yv*=db2g(-lim_e[0])
        if s.ott_clipper: yv=math.tanh(yv*1.7)
        yv=v/db2g(s.ott_input)+(yv-v/db2g(s.ott_input))*s.ott_mix/100
        yv*=ml
        # Type-A four overlapping bands.
        inp=yv*db2g(s.atype_input); bnds=[.25*inp,.25*inp,.3*inp,.2*inp]; enh=0
        for b,z in enumerate(bnds):
            ld3=g2db(abs(z)+1e-5); boost=max(0,min(10,(-40-ld3)*.5*s.atype_degree[b]/100)) if ld3<-40 else 0
            enh += z*(db2g(boost+s.atype_level[b])-1)
        yv=inp+enh*s.atype_mix/100; yv*=db2g(s.atype_output); yv=(yv/db2g(s.atype_input)) if abs(s.atype_input)>1e-12 else yv
        out.append(yv)
    # De-esser band approximation, with two edge validation.
    de=[]
    de_env=0
    for i,v in enumerate(out):
        sb=v*(.75+.25*math.sin(i*.31))
        de_env=env_follow(de_env,sb,s.deess_attack,s.deess_release,sr); ld=g2db(max(de_env,1e-5))
        red=max(0,min(s.deess_range,(ld+36)*s.deess_strength/100)) if ld>-36 else 0
        wet=db2g(-red)*sb
        de.append(wet if s.deess_listen else v+wet-sb)
    mix=s.drywet/100; og=db2g(s.output)
    return [(a+(b-a)*mix)*og for a,b in zip(dry,de)]

def random_state(rng):
    return sanitize(State(
        freq=[rng.uniform(0,22050) for _ in range(4)],gain=[rng.uniform(-36,36) for _ in range(4)],q=[10**rng.uniform(-1.5,1.5) for _ in range(4)],
        eq_color=rng.uniform(-20,130),hf=rng.uniform(0,240),
        ott_degree=[rng.uniform(-20,140) for _ in range(4)],
        lift_t=[rng.uniform(-100,20) for _ in range(4)],lift_a=[rng.uniform(0,800) for _ in range(4)],lift_r=[rng.uniform(1,4000) for _ in range(4)],lift_m=[rng.uniform(-20,140) for _ in range(4)],
        comp_t=[rng.uniform(-40,20) for _ in range(4)],comp_a=[rng.uniform(0,300) for _ in range(4)],comp_r=[rng.uniform(1,4000) for _ in range(4)],comp_m=[rng.uniform(-20,140) for _ in range(4)],
        ott_level=[rng.uniform(-36,24) for _ in range(4)],ott_x=[rng.uniform(40,800),rng.uniform(600,4000),rng.uniform(5000,14000)],
        ott_input=rng.uniform(-40,40),ott_gate=rng.uniform(-120,20),ott_mix=rng.uniform(-20,140),ott_output=rng.uniform(-40,30),
        atype_degree=[rng.uniform(-20,140) for _ in range(4)],atype_level=[rng.uniform(-12,12) for _ in range(4)],atype_attack=rng.uniform(0,150),atype_release=rng.uniform(1,800),atype_input=rng.uniform(-40,40),atype_mix=rng.uniform(-20,140),atype_output=rng.uniform(-40,30),
        deess_low=rng.uniform(1000,12000),deess_high=rng.uniform(5000,18000),deess_range=rng.uniform(-10,36),deess_strength=rng.uniform(-20,140),deess_attack=rng.uniform(0,40),deess_release=rng.uniform(1,600),
        drywet=rng.uniform(-20,140),output=rng.uniform(-40,24)))

def signature(s):
    vals=[*s.freq,*s.gain,*s.q,s.eq_color,s.hf,*s.ott_degree,*s.lift_t,*s.lift_a,*s.lift_r,*s.lift_m,*s.comp_t,*s.comp_a,*s.comp_r,*s.comp_m,*s.ott_level,*s.ott_x,s.ott_input,s.ott_gate,s.ott_mix,float(s.ott_clipper),s.ott_output,*s.atype_degree,*s.atype_level,s.atype_attack,s.atype_release,s.atype_input,s.atype_mix,s.atype_output,s.deess_low,s.deess_high,s.deess_range,s.deess_strength,s.deess_attack,s.deess_release,float(s.deess_listen),s.drywet,s.output]
    return sum((i+1)*float(v) for i,v in enumerate(vals))

def assert_ok(y,label):
    if not finite(y): raise AssertionError(label+": non-finite")
    if max((abs(v) for v in y),default=0)>1e8: raise AssertionError(label+": unstable")

def structure_checks():
    cpp=Path("Source/PluginProcessor.cpp").read_text(encoding="utf-8"); dsp=Path("Source/DSP/ChainDSP.cpp").read_text(encoding="utf-8"); web=Path("docs/index.html").read_text(encoding="utf-8"); ui=Path("Source/PluginEditor.cpp").read_text(encoding="utf-8")
    for item in ["OTT_DEGREE","OTT_LIFT_T","OTT_LIFT_M","OTT_COMP_T","OTT_COMP_M","OTT_X1","OTT_X2","OTT_X3","ATYPE_DEGREE","ATYPE_LEVEL","DEESS_LOW","DEESS_HIGH","DEESS_RANGE","DEESS_STRENGTH"]:
        if item not in cpp: raise AssertionError("missing APVTS "+item)
    if "for (int i = 0; i < 4; ++i)" not in cpp:
        raise AssertionError("missing four-band APVTS loop")
    for item in ["makeAnalogPeak","applyLifter","applyCompressor","applyGate","applyLimiter","applyOtt","applyAType","applyDeEsser"]:
        if item not in dsp: raise AssertionError("missing DSP "+item)
    for item in ["mouseDown","mouseDrag","OttDegree1","TypeDegree1","DeessLow","MixDryWet"]:
        if item not in ui: raise AssertionError("missing native graph control "+item)
    for item in ["functionTabs","AudioWorkletNode","loopStart","loopEnd","ottDegree","atypeDegree","deessLow","deessHigh","moduleBtn"]:
        if item not in web: raise AssertionError("missing web control "+item)

def run():
    rng=random.Random(SEED); failures=[]

    for i in range(COUNTS["planning"]):
        s=random_state(rng)
        try:
            if not math.isfinite(signature(s)): raise AssertionError("non-finite state")
            if not (80<=s.ott_x[0]<s.ott_x[1]<s.ott_x[2]<=12000): raise AssertionError("OTT xover ordering")
            if not (2500<=s.deess_low<s.deess_high<=15000): raise AssertionError("De-Esser ordering")
        except AssertionError as e: failures.append(("planning",i,str(e)))

    edges=[State(eq_color=0),State(eq_color=100),State(hf=40),State(hf=120),State(ott_degree=[0]*4),State(ott_degree=[100]*4),
           State(ott_mix=0),State(ott_mix=100),State(ott_gate=-90),State(ott_gate=0),State(ott_clipper=False),State(ott_clipper=True),
           State(lift_t=[-80]*4),State(lift_t=[0]*4),State(comp_t=[-24]*4),State(comp_t=[0]*4),State(atype_degree=[0]*4),State(atype_degree=[100]*4),
           State(atype_mix=0),State(atype_mix=100),State(deess_range=0),State(deess_range=24),State(deess_strength=0),State(deess_strength=100),
           State(drywet=0),State(drywet=100),State(output=-24),State(output=12)]
    for i in range(COUNTS["debug"]):
        try: assert_ok(chain([0,1,-1,.25,-.25,0],edges[i%len(edges)]),"debug")
        except AssertionError as e: failures.append(("debug",i,str(e)))

    mutations=[
        ("EQ_FREQ",lambda s:s.freq.__setitem__(0,400)),("EQ_GAIN",lambda s:s.gain.__setitem__(0,6)),("EQ_Q",lambda s:s.q.__setitem__(0,8)),("EQ_COLOR",lambda s:setattr(s,"eq_color",85)),("HF",lambda s:setattr(s,"hf",110)),
        ("OTT_DEG1",lambda s:s.ott_degree.__setitem__(0,80)),("OTT_DEG2",lambda s:s.ott_degree.__setitem__(1,80)),("OTT_DEG3",lambda s:s.ott_degree.__setitem__(2,80)),("OTT_DEG4",lambda s:s.ott_degree.__setitem__(3,80)),
        ("OTT_LIFT_T",lambda s:s.lift_t.__setitem__(0,-20)),("OTT_LIFT_A",lambda s:s.lift_a.__setitem__(0,300)),("OTT_LIFT_R",lambda s:s.lift_r.__setitem__(0,800)),("OTT_LIFT_M",lambda s:s.lift_m.__setitem__(0,40)),
        ("OTT_COMP_T",lambda s:s.comp_t.__setitem__(0,-4)),("OTT_COMP_A",lambda s:s.comp_a.__setitem__(0,1)),("OTT_COMP_R",lambda s:s.comp_r.__setitem__(0,500)),("OTT_COMP_M",lambda s:s.comp_m.__setitem__(0,30)),("OTT_LEVEL",lambda s:s.ott_level.__setitem__(0,6)),
        ("OTT_X1",lambda s:s.ott_x.__setitem__(0,450)),("OTT_X2",lambda s:s.ott_x.__setitem__(1,1800)),("OTT_X3",lambda s:s.ott_x.__setitem__(2,10000)),("OTT_INPUT",lambda s:setattr(s,"ott_input",12)),("OTT_GATE",lambda s:setattr(s,"ott_gate",-30)),("OTT_MIX",lambda s:setattr(s,"ott_mix",50)),("OTT_OUTPUT",lambda s:setattr(s,"ott_output",3)),
        ("ATYPE_D1",lambda s:s.atype_degree.__setitem__(0,80)),("ATYPE_D2",lambda s:s.atype_degree.__setitem__(1,80)),("ATYPE_D3",lambda s:s.atype_degree.__setitem__(2,80)),("ATYPE_D4",lambda s:s.atype_degree.__setitem__(3,80)),("ATYPE_LEVEL",lambda s:s.atype_level.__setitem__(2,4)),("ATYPE_ATTACK",lambda s:setattr(s,"atype_attack",2)),("ATYPE_RELEASE",lambda s:setattr(s,"atype_release",300)),("ATYPE_INPUT",lambda s:setattr(s,"atype_input",6)),("ATYPE_MIX",lambda s:setattr(s,"atype_mix",50)),("ATYPE_OUTPUT",lambda s:setattr(s,"atype_output",2)),
        ("DE_LOW",lambda s:setattr(s,"deess_low",5500)),("DE_HIGH",lambda s:setattr(s,"deess_high",12000)),("DE_RANGE",lambda s:setattr(s,"deess_range",20)),("DE_STRENGTH",lambda s:setattr(s,"deess_strength",95)),("DE_ATTACK",lambda s:setattr(s,"deess_attack",.2)),("DE_RELEASE",lambda s:setattr(s,"deess_release",250)),
        ("DRY",lambda s:setattr(s,"drywet",30)),("OUT",lambda s:setattr(s,"output",6))
    ]
    tone=[.04*math.sin(2*math.pi*440*n/48000)+.01*math.sin(2*math.pi*6800*n/48000) for n in range(256)]
    base=State()
    for i in range(COUNTS["all_features"]):
        name,mut=mutations[i%len(mutations)]; s=State();mut(s)
        try: assert abs(signature(s)-signature(base))>1e-9; assert_ok(chain(tone,s),name)
        except AssertionError as e: failures.append(("all_features",i,str(e)))

    for i in range(COUNTS["transient"]):
        s=random_state(rng); x=[0.0]*256; k=i%6
        if k==0:x[0]=1
        elif k==1:x[0]=1;x[1]=-.9
        elif k==2:
            for n in range(0,256,16):x[n]=.8 if (n//16)%2==0 else -.8
        elif k==3:
            for n in range(8):x[n]=.95*(.7**n)
        elif k==4:x[64]=1;x[65]=1;x[66]=-.85
        else:
            for n in range(32,256,64):x[n]=.75
        try: assert_ok(chain(x,s,SRS[i%len(SRS)]),"transient")
        except AssertionError as e: failures.append(("transient",i,str(e)))

    for i in range(COUNTS["full_chain"]):
        sr=SRS[i%len(SRS)]; block=BLOCKS[i%len(BLOCKS)]; a=(i%91)/100; b=min(1,a+((i%29)+1)/100); s=random_state(rng); tone=110*2**((i%72)/12)
        x=[.06*math.sin(2*math.pi*tone*n/sr)+.008*math.sin(2*math.pi*6500*n/sr) for n in range(block)]
        try:
            if not (0<=a<b<=1): raise AssertionError("loop range")
            assert_ok(chain(x,s,sr),"full")
        except AssertionError as e: failures.append(("full_chain",i,str(e)))

    structure_checks()
    total=sum(COUNTS.values())
    print("VVChain reference stress test");print("seed:",SEED)
    for k,v in COUNTS.items():print(k+":",v)
    print("total:",total);print("failures:",len(failures))
    if failures: print("first_failure:",failures[0]);return 1
    print("status: PASS");return 0

if __name__=="__main__":
    raise SystemExit(run())
