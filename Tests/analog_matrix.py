import numpy as np

def static_transfer(x, amount, solid_state):
    x=np.asarray(x,dtype=np.float64); amount=float(np.clip(amount,0.0,0.60))
    if amount<=0.0:return x.copy()
    alpha=amount*(1.80 if solid_state else 1.55); norm=(1.0+alpha)**0.25
    u=np.clip(x,-1.0,1.0)
    return u/(1.0+alpha*u*u)**0.25*norm

def adaa_reference(x, amount, solid_state):
    x=np.asarray(x,dtype=np.float64); amount=float(np.clip(amount,0.0,0.60))
    if x.size==0 or amount<=0.0:return x.copy()
    alpha=amount*(1.80 if solid_state else 1.55); norm=(1.0+alpha)**0.25
    def f(v):return v/(1.0+alpha*v*v)**0.25*norm
    def F(v):return (2.0/(3.0*alpha))*((1.0+alpha*v*v)**0.75-1.0)*norm
    shaped=np.clip(x,-1.0,1.0);y=np.empty_like(shaped);prev=0.0;has=False
    for i,sample in enumerate(shaped):
        if not has:out=f(sample);has=True
        else:
            d=sample-prev;out=f(0.5*(sample+prev)) if abs(d)<1e-7 else (F(sample)-F(prev))/d
        y[i]=out;prev=sample
    return y

def analog_reference(x,amount,solid_state,x2=1.0):
    x=np.asarray(x,dtype=np.float64);amount=float(np.clip(amount,0.0,0.60));x2=float(np.clip(x2,1.0,2.0))
    if amount<=0.0:return x.copy()
    shaped=np.clip(x,-1.0,1.0);return x+(adaa_reference(x,amount,solid_state)-shaped)*x2

def run():
    from pathlib import Path
    cpp=Path("Source/DSP/ChainDSP.cpp").read_text(encoding="utf-8");h=Path("Source/DSP/ChainDSP.h").read_text(encoding="utf-8");ah=Path("Source/DSP/VVChain_AnalogADAA_v2.h").read_text(encoding="utf-8")
    for marker in ('#include "VVChain_AnalogADAA_v2.h"',"const double modeAlpha = solidState ? 1.80 : 1.55;","const double shapingInput = juce::jlimit(-1.0, 1.0, x);","analogADAA[band][ch].processSample","const double delta = saturated - shapingInput","x + delta * static_cast<double>(safeColourMultiplier)","constexpr double kSmoothingMs = 0.25;"):assert marker in cpp,marker
    assert "std::array<std::array<VVChain_AnalogADAA_v2, 2>, 4> analogADAA" in h
    for marker in ("calcAntiderivative","m_hasPrev","std::sqrt(std::sqrt(1.0 + alpha))","(u075 - 1.0)"):assert marker in ah,marker
    rng=np.random.default_rng(20260924);rates=[44100.0,48000.0,88200.0,96000.0];min_tt_ss=np.inf;max_out=0.0
    for case in range(500):
        fs=rates[case%4];amount=.60*(((case*37)%1001)/1000.0);ss=bool(case&1);n=8192;t=np.arange(n)/fs;freq=20+((case*43)%int(min(18000,fs*.4)));amp=.02+.95*((case*71)%1000)/999
        kind=case%5
        if kind==0:x=amp*np.sin(2*np.pi*freq*t)
        elif kind==1:
            f2=min(freq*1.73,fs*.45);x=.72*amp*np.sin(2*np.pi*freq*t+.17)+.21*amp*np.sin(2*np.pi*f2*t+.91)
        elif kind==2:x=.35*amp*rng.standard_normal(n)
        elif kind==3:x=amp*np.sign(np.sin(2*np.pi*freq*t))
        else:x=amp*np.linspace(-1,1,n)
        y=analog_reference(x,amount,ss);assert np.all(np.isfinite(y));max_out=max(max_out,float(np.max(np.abs(y))))
        if amount==0.0:assert np.array_equal(y,x)
        p=np.linspace(-1,1,4097);sy=static_transfer(p,amount,ss)
        if amount>0:assert np.min(np.abs(sy)-np.abs(p))>=-1e-12;assert np.max(np.abs(static_transfer(np.array([-1.,0.,1.]),amount,ss)-np.array([-1.,0.,1.])))<=1e-12
        tt=analog_reference(x,max(amount,.01),False);ssy=analog_reference(x,max(amount,.01),True);min_tt_ss=min(min_tt_ss,float(np.max(np.abs(tt-ssy))))
        b=analog_reference(x[:1024],amount,ss,1);z=analog_reference(x[:1024],amount,ss,2);assert np.max(np.abs((z-x[:1024])-(b-x[:1024])*2))<1e-9
        pair=np.concatenate([x,-x]);yp=analog_reference(pair,amount,ss);assert np.max(np.abs(yp[:n]+yp[n:]))<1e-9
    for ss in (False,True):
        amount=.60;alpha=amount*(1.80 if ss else 1.55);norm=(1+alpha)**.25;x=.437;direct=x/(1+alpha*x*x)**.25*norm;assert abs(adaa_reference(np.array([x]),amount,ss)[0]-direct)<1e-12
    assert min_tt_ss>1e-7
    print(f"PASS ANALOG v1.0.45 500-case matrix: cases=500, min_tt_ss_delta={min_tt_ss:.3e}, max_output={max_out:.6f}")

if __name__=="__main__":run()
