#!/usr/bin/env python3
from __future__ import annotations
import math, random
from dataclasses import dataclass, field

SR_CHOICES=[44100,48000,88200,96000,192000]
BLOCKS=[16,32,64,128,256,512,1024]

def clamp(x,lo,hi): return max(lo,min(hi,x))

@dataclass
class State:
    freq:list[float]=field(default_factory=lambda:[80,350,2500,10000])
    gain:list[float]=field(default_factory=lambda:[0,0,0,0])
    q:list[float]=field(default_factory=lambda:[.707]*4)
    hf:float=70
    ott_depth:float=50
    ott_mix:float=50
    atype:float=20
    bias:float=0
    deess_freq:float=6500
    deess_threshold:float=-30
    deess_range:float=8
    drywet:float=100
    output_db:float=0

def sanitize(s):
    s.freq=[clamp(v,20,20000) for v in s.freq]
    s.gain=[clamp(v,-24,24) for v in s.gain]
    s.q=[clamp(v,.10,18) for v in s.q]
    s.hf=clamp(s.hf,40,120)
    s.ott_depth=clamp(s.ott_depth,0,100)
    s.ott_mix=clamp(s.ott_mix,0,100)
    s.atype=clamp(s.atype,0,100)
    s.bias=clamp(s.bias,-100,100)
    s.deess_freq=clamp(s.deess_freq,2000,12000)
    s.deess_threshold=clamp(s.deess_threshold,-80,0)
    s.deess_range=clamp(s.deess_range,0,24)
    s.drywet=clamp(s.drywet,0,100)
    s.output_db=clamp(s.output_db,-24,12)
    return s

def finite_state(s):
    return all(math.isfinite(v) for v in s.freq+s.gain+s.q+
        [s.hf,s.ott_depth,s.ott_mix,s.atype,s.bias,s.deess_freq,
         s.deess_threshold,s.deess_range,s.drywet,s.output_db])

def process(samples,s,sr=48000):
    s=sanitize(s)
    hp_alpha=math.exp(-2*math.pi*s.hf/sr)
    prev=0.0
    env=0.0
    out=[]
    for x in samples:
        hp=x-hp_alpha*prev
        prev=x
        y=hp
        for f,g,q in zip(s.freq,s.gain,s.q):
            influence=math.exp(-((math.log(max(f,20)/max(20,f+abs(2*math.pi*f/sr))))**2)*2)
            influence=clamp(influence*(1+min(q,18)/36),0,1)
            y*=10**((g*influence)/20)
        ax=abs(y)
        coeff=.004 if ax>env else .0004
        env+=coeff*(ax-env)
        down=clamp((env-.16)*3.5,0,1)
        up=clamp((.16-env)*2,0,1)
        depth=s.ott_depth/100
        wet=y*(.55**(down*depth))*(1+1.4*up*depth)
        y+=s.ott_mix/100*(wet-y)
        amt=s.atype/100
        yy=y+s.bias/100*.003
        y+=amt*.35*(math.tanh(yy*(1+2.2*amt))-yy)
        if abs(y)>10**(s.deess_threshold/20):
            y*=10**(-s.deess_range/20)
        out.append(y)
    mix=s.drywet/100
    gain=10**(s.output_db/20)
    return [(d+mix*(w-d))*gain for d,w in zip(samples,out)]

def random_state(rng):
    return sanitize(State(
        freq=[rng.uniform(10,22000) for _ in range(4)],
        gain=[rng.uniform(-36,36) for _ in range(4)],
        q=[10**rng.uniform(-1.5,1.5) for _ in range(4)],
        hf=rng.uniform(10,200), ott_depth=rng.uniform(-20,130),
        ott_mix=rng.uniform(-20,130), atype=rng.uniform(-20,130),
        bias=rng.uniform(-140,140), deess_freq=rng.uniform(1000,15000),
        deess_threshold=rng.uniform(-120,20), deess_range=rng.uniform(-10,40),
        drywet=rng.uniform(-20,140), output_db=rng.uniform(-40,24)))

def run():
    rng=random.Random(20260920)
    counts={"planning_matrix":0,"debug_regression":0,"all_features":0,"transient":0,"full_chain":0}
    failures=[]
    for i in range(1280):
        if not finite_state(random_state(rng)): failures.append(("planning",i))
        counts["planning_matrix"]+=1
    extremes=[State(gain=[-24]*4),State(gain=[24]*4),State(q=[.1]*4),
              State(q=[18]*4),State(hf=40),State(hf=120),State(drywet=0),
              State(drywet=100),State(output_db=-24),State(output_db=12),
              State(deess_threshold=-80),State(deess_threshold=0)]
    for i in range(120):
        y=process([0,1,-1,.25,-.25],extremes[i%len(extremes)])
        if not all(math.isfinite(v) for v in y): failures.append(("debug",i))
        counts["debug_regression"]+=1
    for i in range(180):
        s=random_state(rng); s.drywet=(i%11)*10; s.ott_mix=(i%13)*100/12
        s.atype=(i%9)*100/8; s.deess_range=(i%7)*4
        y=process([math.sin(2*math.pi*440*n/48000) for n in range(64)],s)
        if not all(math.isfinite(v) for v in y): failures.append(("all_features",i))
        counts["all_features"]+=1
    for i in range(655):
        s=random_state(rng); x=[0.0]*256; k=i%4
        if k==0:x[0]=1
        elif k==1:x[0]=1;x[1]=-.7
        elif k==2:x[20]=1;x[21]=1;x[22]=-.8
        else:
            for n in range(0,128,16): x[n]=.8 if (n//16)%2==0 else -.8
        y=process(x,s)
        if not all(math.isfinite(v) for v in y) or max(map(abs,y))>1e8:
            failures.append(("transient",i))
        counts["transient"]+=1
    for i in range(820):
        sr=SR_CHOICES[i%len(SR_CHOICES)]; block=BLOCKS[i%len(BLOCKS)]
        s=random_state(rng); tone=220*(2**((i%48)/12))
        x=[.07*math.sin(2*math.pi*tone*n/sr)+.015*rng.uniform(-1,1) for n in range(block)]
        y=process(x,s,sr)
        if not all(math.isfinite(v) for v in y): failures.append(("full_chain",i))
        counts["full_chain"]+=1
    print("VVChain reference stress test")
    print("Seed: 20260920")
    for k,v in counts.items(): print(f"{k}: {v}")
    print("failures:",len(failures))
    print("status:","PASS" if not failures else "FAIL")
    return 0 if not failures else 1

if __name__=="__main__":
    raise SystemExit(run())
