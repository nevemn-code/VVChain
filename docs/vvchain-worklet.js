// VVChain Web AudioWorklet DSP module · v1.0.60
class VVChainWorklet extends AudioWorkletProcessor {
  constructor(){
    super();
    this.ready=false; this.s=null;
    this.N=512;
    this.masterBlend=0;
    this.soloBlend=0;
    this._lastSoloBand=-9;
    this._lastSoloPost=false;
    this._meterBlocks=0;
    this._sentReady=false;
    this.pendingState=null;
    this.analogAlpha=[0,0,0,0];
    this.analogAlphaInitialized=[false,false,false,false];
    this.analogMode=[false,false,false,false];
    this.transientFast=[0,0,0,0];
    this.transientSlow=[0,0,0,0];
    this.transientGain=[1,1,1,1];
    this.transientInit=[false,false,false,false];
    this.pendingRevision=0;
    this.activeRevision=0;
    this._errorReported=false;
    this.ch=[this.makeCh(),this.makeCh()];
    this.port.onmessage=e=>{
      if(!e.data)return;
      if(e.data.type!=="params")return;
      const next=e.data.state;
      if(!next||typeof next!=="object")return;
      // Keep only the newest parameter snapshot. Older drag events are
      // obsolete by definition and must not queue behind the realtime audio.
      const previous=this.pendingState||this.s;
      if(previous&&(
        Number(previous.solo?.band??-1)!==Number(next.solo?.band??-1) ||
        !!previous.solo?.post!==!!next.solo?.post))
        this.soloBlend=0;
      this.pendingState=next;
      this.pendingRevision=Number(e.data.revision||0);
      this.ready=true;
    };
  }
  makeCh(){
    const eqFilter=()=>({
      type:-1,tpt:{g:0,k:1,a1:1,a2:0,a3:0,m1:0,ic1:0,ic2:0},
      stages:Array.from({length:12},()=>({z1:0,z2:0})),
      last:0,start:0,transition:1
    });
    const dynState=()=>({det:{z1:0,z2:0},eq:eqFilter(),env:-120});
    return {
      eq:Array.from({length:4},eqFilter),
      dynMid:Array.from({length:4},dynState),
      dynSide:Array.from({length:4},dynState),
      analogAd:Array.from({length:4},()=>({prevX:0,hasPrev:false})),
      bandProcessingHp:{z1:0,z2:0},
      analogLp:[0,0,0], lp:[0,0,0], typeLp:[0,0,0], gate:0, gateBand:[0,0,0,0], lim:0,
      lift:[1,1,1,1], comp:[0,0,0,0], typeFast:[0,0,0,0], typeSlow:[0,0,0,0], typeDc:[0,0,0,0],
      transientLp:[0,0,0], transientHp:{z1:0,z2:0}, soloPre:[0,0,0], soloPost:[0,0,0], graphSoloPre:{z1:0,z2:0}, graphSoloPost:{z1:0,z2:0}
    };
  }
  clamp(v,a,b){return Math.max(a,Math.min(b,v))}
  finite(v){return Number.isFinite(v)?v:0}
  db2g(db){return Math.pow(10,db/20)}
  g2db(g){return 20*Math.log10(Math.max(g,1e-9))}
  tc(ms){return Math.exp(-1/(.001*Math.max(.1,ms)*sampleRate))}
  biquad(x,c,z){const y=c[0]*x+z.z1;z.z1=c[1]*x-c[3]*y+z.z2;z.z2=c[2]*x-c[4]*y;return y}
  tptBell(x,z,fs,f,q,gainDb){
    const safeF=this.clamp(Number(f),20,fs*.45);
    const safeQ=this.clamp(Number(q),.1,18);
    const safeGain=this.clamp(Number(gainDb),-18,18);
    // Defensive state initialization: Dynamic Bell state must always remain finite,
    // including snapshots created by older Web versions.
    z.ic1=Number.isFinite(z.ic1)?z.ic1:0;
    z.ic2=Number.isFinite(z.ic2)?z.ic2:0;
    const A=Math.pow(10,safeGain/40);
    const g=Math.tan(Math.PI*safeF/fs);
    const k=1/(safeQ*A);
    z.g=g; z.k=k;
    z.a1=1/(1+g*(g+k)); z.a2=g*z.a1; z.a3=g*z.a2;
    z.m1=k*(A*A-1);
    const v3=x-z.ic2;
    const v1=z.a1*z.ic1+z.a2*v3;
    const v2=z.ic2+z.a2*z.ic1+z.a3*v3;
    z.ic1=2*v1-z.ic1; z.ic2=2*v2-z.ic2;
    return x+z.m1*v1;
  }
  peak(fs,f,q,g){
    const A=Math.pow(10,g/40),w=2*Math.PI*this.clamp(f,10,fs*.45)/fs;
    const a=Math.sin(w)/(2*Math.max(.1,q)),cc=Math.cos(w);
    const b0=1+a*A,b1=-2*cc,b2=1-a*A,a0=1+a/A,a1=-2*cc,a2=1-a/A;
    return[b0/a0,b1/a0,b2/a0,a1/a0,a2/a0]
  }
  bp(f,q=.707){
    const w=2*Math.PI*this.clamp(f,10,sampleRate*.45)/sampleRate;
    const sn=Math.sin(w),cc=Math.cos(w),a=sn/(2*Math.max(.1,q));
    const b0=sn/2,b1=0,b2=-sn/2,a0=1+a,a1=-2*cc,a2=1-a;
    return[b0/a0,b1/a0,b2/a0,a1/a0,a2/a0]
  }
  lp(f,q=.707){
    const w=2*Math.PI*this.clamp(f,10,sampleRate*.45)/sampleRate;
    const a=Math.sin(w)/(2*Math.max(.1,q)),cc=Math.cos(w);
    const b0=(1-cc)/2,b1=1-cc,b2=(1-cc)/2,a0=1+a,a1=-2*cc,a2=1-a;
    return[b0/a0,b1/a0,b2/a0,a1/a0,a2/a0]
  }
  hp(f,q=.707){
    const w=2*Math.PI*this.clamp(f,10,sampleRate*.45)/sampleRate;
    const a=Math.sin(w)/(2*Math.max(.1,q)),cc=Math.cos(w);
    const b0=(1+cc)/2,b1=-(1+cc),b2=(1+cc)/2;
    const a0=1+a,a1=-2*cc,a2=1-a;
    return[b0/a0,b1/a0,b2/a0,a1/a0,a2/a0]
  }
  lp1(f){
    const sf=this.clamp(f,10,sampleRate*.45);
    const K=Math.tan(Math.PI*sf/sampleRate),inv=1/(1+K);
    return[K*inv,K*inv,0,(K-1)*inv,0]
  }
  hp1(f){
    const sf=this.clamp(f,10,sampleRate*.45);
    const K=Math.tan(Math.PI*sf/sampleRate),inv=1/(1+K);
    return[inv,-inv,0,(K-1)*inv,0]
  }

  notch(f,q=.707){
    const w=2*Math.PI*this.clamp(f,10,sampleRate*.45)/sampleRate;
    const sn=Math.sin(w),cc=Math.cos(w),a=sn/(2*Math.max(.1,q)),a0=1+a;
    return[1/a0,-2*cc/a0,1/a0,-2*cc/a0,(1-a)/a0]
  }
  shelf(high,f,gain,slope=1){
    const sf=this.clamp(f,20,sampleRate*.45),A=Math.pow(10,gain/40);
    const w=2*Math.PI*sf/sampleRate,sn=Math.sin(w),cs=Math.cos(w),S=this.clamp(slope,.1,1);
    const rootA=Math.sqrt(A),term=Math.max(0,(A+1/A)*(1/S-1)+2);
    const alpha=.5*sn*Math.sqrt(term),beta=2*rootA*alpha;
    let b0,b1,b2,a0,a1,a2;
    if(!high){
      b0=A*((A+1)-(A-1)*cs+beta);
      b1=2*A*((A-1)-(A+1)*cs);
      b2=A*((A+1)-(A-1)*cs-beta);
      a0=(A+1)+(A-1)*cs+beta;
      a1=-2*((A-1)+(A+1)*cs);
      a2=(A+1)+(A-1)*cs-beta;
    }else{
      b0=A*((A+1)+(A-1)*cs+beta);
      b1=-2*A*((A-1)+(A+1)*cs);
      b2=A*((A+1)+(A-1)*cs-beta);
      a0=(A+1)-(A-1)*cs+beta;
      a1=2*((A-1)-(A+1)*cs);
      a2=(A+1)-(A-1)*cs-beta;
    }
    return[b0/a0,b1/a0,b2/a0,a1/a0,a2/a0]
  }
  resetEqFilter(z,type){
    if(z.type===type)return;
    z.start=Number.isFinite(z.last)?z.last:0;z.transition=0;z.type=type;
    z.tpt={g:0,k:1,a1:1,a2:0,a3:0,m1:0,ic1:0,ic2:0};
    z.stages.forEach(s=>{s.z1=0;s.z2=0});
  }
  eqFilter(x,z,type,f,q,gainDb,slopeIndex=1){
    type=this.clamp(Math.round(Number(type)||0),0,13);
    if(type===3)type=2;
    else if(type===1||type===10||type===11)type=0;
    this.resetEqFilter(z,type);
    const sf=this.clamp(Number(f)||1000,20,sampleRate*.45);
    const qq=this.clamp(Number(q)||.707,.1,18);
    const gain=this.clamp(Number(gainDb)||0,-18,18);
    let y=x;
    if(type===0){
      y=this.tptBell(x,z.tpt,sampleRate,sf,qq,gain);
    }else{
      let coefs=[],parallel=false,mix=0,outGain=1;
      if(type===1){
        coefs=[this.peak(sampleRate,sf,qq,gain)];
      }else if(type===2||type===3){
        const bw=this.clamp(1.4/Math.sqrt(qq),.2,4),ratio=Math.pow(2,bw*.5);
        const lo=this.clamp(sf/ratio,20,sampleRate*.44),hi=this.clamp(sf*ratio,lo*1.02,sampleRate*.45);
        parallel=true;mix=Math.pow(10,gain/20)-1;
        if(type===2){
          const edgeQ=this.clamp(qq,.45,2.5);
          coefs=[this.hp(lo,edgeQ),this.lp(hi,edgeQ)];
        }else{
          const butter=[.5043144803,.5411961001,.630236207, .8213398159,1.3065629649,3.8306487878];
          const scale=this.clamp(qq/.7071067811865476,.35,2.5);
          for(let i=0;i<6;i++)coefs.push(this.hp(lo,this.clamp(butter[i]*scale,.25,12)));
          for(let i=0;i<6;i++)coefs.push(this.lp(hi,this.clamp(butter[i]*scale,.25,12)));
        }
      }else if(type===4||type===5){
        coefs=[this.shelf(type===5,sf,gain,1)];
      }else if(type===6||type===7){
        const rg=(gain>=0?1:-1)*Math.min(6,Math.abs(gain)*.35);
        coefs=[this.shelf(type===7,sf,gain,1),this.peak(sampleRate,sf,qq,rg)];
      }else if(type===8||type===9){
        coefs=[this.shelf(type===9,sf,gain,.28)];
      }else if(type===10){
        coefs=[this.bp(sf,qq)];outGain=Math.pow(10,gain/20);
      }else if(type===11){
        coefs=[this.notch(sf,qq)];
      }else{
        const idx=this.clamp(Math.round(Number(slopeIndex??1)),0,6);
        if(idx===0){
          coefs=[type===12?this.lp1(sf):this.hp1(sf)];
        }else{
          const sections=this.clamp(idx,1,6),order=sections*2;
          for(let i=0;i<sections;i++){
            const angle=(2*i+1)*Math.PI/(2*order);
            const rq=1/(2*Math.cos(angle));
            coefs.push(type===12?this.lp(sf,rq):this.hp(sf,rq));
          }
        }
      }
      for(let i=0;i<coefs.length;i++)y=this.biquad(y,coefs[i],z.stages[i]);
      y=parallel?x+mix*y:y*outGain;
    }
    if(z.transition<1){
      z.transition=Math.min(1,z.transition+1/64);
      y=z.start*(1-z.transition)+y*z.transition;
    }
    z.last=Number.isFinite(y)?y:0;
    return z.last;
  }
  dynamicStereo(l,r,stereo){
    const s=this.s,c=this.ch[0];
    if(s.eq.bypass||s.dyn?.bypass)return[l,r];
    const invSqrt2=0.7071067811865476;
    let mid=stereo?(l+r)*invSqrt2:l;
    let side=stereo?(l-r)*invSqrt2:0;
    const knee=10;
    for(let b=0;b<4;b++){
      if(s.bandBypass?.[b])continue;
      const f=this.clamp(Number(s.eq.freq[b]||1000),20,20000);
      const baseQ=this.clamp(Number(s.eq.q[b]||.707),.1,18);
      let eqType=this.clamp(Math.round(Number(s.eq.type?.[b]||0)),0,13);
      if(eqType===3)eqType=2;
      else if(eqType===1||eqType===10||eqType===11)eqType=0;
      if((b===1||b===2)&&eqType>=12)eqType=0;
      const detectorQ=eqType>=12?.70710678:baseQ;
      const offset=this.clamp(Number(s.eq.gain[b]||0),-18,18);
      const dynamicRangeDb=18;
      const dynamicsSigned=this.clamp(Number(s.dyn.dynamics[b]??0),-100,100)/100;
      const dynamicsAmount=Math.abs(dynamicsSigned);
      const dynamicsDirection=dynamicsSigned>=0?1:-1;
      const dynAbs=Math.pow(Math.abs(dynamicsSigned),0.65);
      const thr=this.clamp(-12+(dynamicsSigned<0?-12:12)*dynAbs,-60,0);
      const attack=this.clamp(Number(s.dyn.attack[b]||5),.1,200);
      const release=this.clamp(Number(s.dyn.release[b]||100),5,2000);
      const ac=this.tc(attack),rc=this.tc(release);
      const ms=this.clamp(Number(s.dyn.ms[b]??50),0,100);
      const mw=this.clamp(ms/50,0,1),sw=this.clamp((100-ms)/50,0,1);
      const bpCoef=this.bp(f,detectorQ);
      const md=c.dynMid[b],sd=c.dynSide[b];
      const dm=this.biquad(mid,bpCoef,md.det);
      const ds=stereo?this.biquad(side,bpCoef,sd.det):0;
      const mDb=this.g2db(Math.abs(dm)+1e-6);
      const sDb=stereo?this.g2db(Math.abs(ds)+1e-6):-120;
      const mt=mDb>md.env?ac*mDb+(1-ac)*md.env:rc*md.env+(1-rc)*mDb;
      const st=sDb>sd.env?ac*sDb+(1-ac)*sd.env:rc*sd.env+(1-rc)*sDb;
      md.env=Number.isFinite(mt)?mt:-120;
      sd.env=Number.isFinite(st)?st:-120;
      const slowTc=this.tc(120);
      md.slow=slowTc*(md.slow??-120)+(1-slowTc)*mDb;
      sd.slow=slowTc*(sd.slow??-120)+(1-slowTc)*sDb;

      const activation=(level,below)=>{
        let a=0;
        if(!below){
          const lo=thr-knee;
          if(level<=lo)a=0;
          else if(level>=thr)a=1;
          else {const t=(level-lo)/knee;a=t*t*(3-2*t);}
        }else{
          const hi=thr+knee;
          if(level>=hi)a=0;
          else if(level<=thr)a=1;
          else {const t=(hi-level)/knee;a=t*t*(3-2*t);}
        }
        return this.clamp(a*dynamicsAmount,0,1);
      };

      let midLevel=md.env,sideLevel=sd.env;
      const onsetMix=this.clamp(Number(s.dyn.detectOnsets?.[b]??50)/100,0,1);
      if(onsetMix>0){
        midLevel+=onsetMix*this.clamp(Math.max(0,mDb-md.slow)*1.5,0,12);
        sideLevel+=onsetMix*this.clamp(Math.max(0,sDb-sd.slow)*1.5,0,12);
      }

      const ma=activation(midLevel,!!s.dyn.triggerBelow?.[b]);
      const sa=activation(sideLevel,!!s.dyn.triggerBelow?.[b]);
      const mtc=ma>(md.activation??0)?ac:rc;
      const stc=sa>(sd.activation??0)?ac:rc;
      md.activation=mtc*(md.activation??0)+(1-mtc)*ma;
      sd.activation=stc*(sd.activation??0)+(1-stc)*sa;

      const delta=dynamicsDirection*dynamicRangeDb;
      const maxUp=this.clamp(18-offset,-18,18);
      const maxDown=this.clamp(-18-offset,-18,18);
      const mChange=this.clamp(delta*md.activation*mw,maxDown,maxUp);
      const sChange=this.clamp(delta*sd.activation*sw,maxDown,maxUp);
      s.dyn.gainMid[b]=.90*Number(s.dyn.gainMid[b]||0)+.10*mChange;
      s.dyn.gainSide[b]=.90*Number(s.dyn.gainSide[b]||0)+.10*sChange;

      const mGain=this.clamp(offset+s.dyn.gainMid[b],-18,18);
      const sGain=this.clamp(offset+s.dyn.gainSide[b],-18,18);
      const mDynamicGain=mGain-offset;
      const sDynamicGain=sGain-offset;
      const slopeIndex=this.clamp(Math.round(Number(s.eq.slope?.[b]??1)),0,6);
      mid=this.eqFilter(mid,md.eq,eqType,f,baseQ,mGain,slopeIndex);
      if(stereo)side=this.eqFilter(side,sd.eq,eqType,f,baseQ,sGain,slopeIndex);
    }
    if(stereo)return[(mid+side)*invSqrt2,(mid-side)*invSqrt2];
    return[mid,r];
  }
  // v1.0.56 analytical first-order ADAA over the same
  // unity-normalized algebraic transfer used by Native.
  analog(x,alpha,ch,b,x2=1){
    alpha=this.clamp(Number(alpha||0),0,1.25);
    const u=this.clamp(Number(x||0),-1,1);
    const st=ch.analogAd[b];
    if(alpha<=1e-5){
      st.prevX=u;st.hasPrev=true;
      return x;
    }
    const norm=Math.sqrt(Math.sqrt(1+alpha));
    const f0=v=>v/Math.sqrt(Math.sqrt(1+alpha*v*v))*norm;
    if(!st.hasPrev){
      st.prevX=u;st.hasPrev=true;
      return f0(u);
    }
    const diff=u-st.prevX;
    let saturated;
    if(Math.abs(diff)<1e-7){
      saturated=f0((u+st.prevX)*0.5);
    }else{
      const F=v=>{
        const uu=1+alpha*v*v;
        const u075=Math.sqrt(uu)*Math.sqrt(Math.sqrt(uu));
        return (2/(3*alpha))*(u075-1)*norm;
      };
      saturated=(F(u)-F(st.prevX))/diff;
    }
    st.prevX=u;
    return this.finite(saturated)
      ? x+(saturated-u)*this.clamp(x2,1,2)
      : x;
  }
  zoneBands(x,c,which,xs){
    const lp=c[which];
    const a1=1-Math.exp(-2*Math.PI*this.clamp(xs[0],40,1000)/sampleRate);
    const a2=1-Math.exp(-2*Math.PI*this.clamp(xs[1],120,5000)/sampleRate);
    const a3=1-Math.exp(-2*Math.PI*this.clamp(xs[2],1000,Math.min(18000,sampleRate*.42))/sampleRate);
    lp[0]+=a1*(x-lp[0]); const h0=x-lp[0];
    lp[1]+=a2*(h0-lp[1]); const h1=h0-lp[1];
    lp[2]+=a3*(h1-lp[2]); const h2=h1-lp[2];
    return [lp[0],lp[1],lp[2],h2];
  }
  applyTransientStereo(l,r,stereo,xs){
    const amounts=Array.isArray(this.s.transient)?this.s.transient:[0,0,0,0];
    const active=[false,false,false,false];
    let any=false;
    for(let b=0;b<4;b++){
      active[b]=Math.abs(Number(amounts[b]||0))>1e-6;
      any=any||active[b];
      if(!active[b]){
        this.transientFast[b]=0;
        this.transientSlow[b]=0;
        this.transientGain[b]=1;
        this.transientInit[b]=false;
      }
    }
    if(!any)return [l,r];

    const bandsL=this.zoneBands(l,this.ch[0],"transientLp",xs);
    const bandsR=stereo?this.zoneBands(r,this.ch[1],"transientLp",xs):bandsL;
    const hp70=this.hp(70,.7071067811865476);
    const fastMs=[2.5,1.5,.8,.35],slowMs=[30,22,15,9];
    const smooth=this.tc(.15);
    const gains=[1,1,1,1];

    for(let b=0;b<4;b++){
      if(!active[b])continue;
      const amount=this.clamp(Number(amounts[b]||0)/100,-1,1);

      let dl=bandsL[b],dr=bandsR[b];
      if(b===0){
        dl=this.biquad(dl,hp70,this.ch[0].transientHp);
        dr=stereo?this.biquad(dr,hp70,this.ch[1].transientHp):dl;
      }

      const energy=stereo?.5*(dl*dl+dr*dr):dl*dl;
      const eps=1e-12;
      if(!this.transientInit[b]){
        const seed=Math.max(energy,eps);
        this.transientFast[b]=seed;
        this.transientSlow[b]=seed;
        this.transientGain[b]=1;
        this.transientInit[b]=true;
      }else{
        const fc=this.tc(fastMs[b]),sc=this.tc(slowMs[b]);
        this.transientFast[b]=fc*this.transientFast[b]+(1-fc)*energy;
        this.transientSlow[b]=sc*this.transientSlow[b]+(1-sc)*energy;
      }

      const ratio=(this.transientFast[b]+eps)/(this.transientSlow[b]+eps);
      const transientDb=(10/Math.LN10)*Math.log(Math.max(ratio,eps));
      const maxDb=12;
      const scaled=transientDb*amount/maxDb;
      const clipped=scaled/(1+Math.abs(scaled));
      const target=this.db2g(clipped*maxDb);
      this.transientGain[b]=smooth*this.transientGain[b]+(1-smooth)*target;
      gains[b]=this.transientGain[b];
    }

    let deltaL=0,deltaR=0;
    for(let b=0;b<4;b++){
      if(!active[b])continue;
      const d=gains[b]-1;
      deltaL+=bandsL[b]*d;
      if(stereo)deltaR+=bandsR[b]*d;
    }
    return [l+deltaL,stereo?r+deltaR:r];
  }

  sample(x,ch,analogAlpha){
    const s=this.s,c=this.ch[ch];let y=x;
    // EQ/Dynamics and the one shared 30 Hz floor are already upstream.

    // ANALOG COLOR v1.0.56: true four-band routing.
    // Shared X1/X2/X3 positions define four bands before independent COLOR/ADAA.
    const analogBands=this.zoneBands(y,c,"analogLp",s.udmbc.x);
    let analogReconstructed=0;
    for(let b=0;b<4;b++){
      const bandInput=analogBands[b];
      if(s.eq.globalBypass||s.eq.colorBypass[b]||analogAlpha[b]<=1e-6){
        analogReconstructed+=bandInput;
      }else{
        const x2=s.eq.colorX2?.[b]?2:1;
        analogReconstructed+=this.analog(
          bandInput,analogAlpha[b],c,b,x2
        );
      }
    }
    y=analogReconstructed;
    if(!s.udmbc.bypass){
      const original=y,inputGain=this.db2g(this.clamp(s.udmbc.input,-24,24)),xs=s.udmbc.x,z=original*inputGain;
      c.lp[0]+=(1-Math.exp(-2*Math.PI*xs[0]/sampleRate))*(z-c.lp[0]);const h0=z-c.lp[0];
      c.lp[1]+=(1-Math.exp(-2*Math.PI*xs[1]/sampleRate))*(h0-c.lp[1]);const h1=h0-c.lp[1];
      c.lp[2]+=(1-Math.exp(-2*Math.PI*xs[2]/sampleRate))*(h1-c.lp[2]);
      const bands=[c.lp[0],c.lp[1],c.lp[2],h1-c.lp[2]];
      for(let b=0;b<4;b++){
        if(s.bandBypass?.[b]||s.udmbc.bandBypass[b])continue;
        const degree=this.clamp(Number(s.udmbc.degree[b]||0),0,100);
        if(degree<=1e-4)continue;
        let v=bands[b];
        const gateDb=this.g2db(Math.abs(v)+1e-9),gt=s.udmbc.gate,knee=9,slope=5;
        const kneeStart=gt-knee/2,kneeEnd=gt+knee/2;let gateTarget=0;
        if(gateDb<kneeStart)gateTarget=(gateDb-gt)*slope;
        else if(gateDb<kneeEnd){const t=this.clamp((gateDb-kneeStart)/knee,0,1);gateTarget=(gateDb-gt)*slope*(1-t)*(1-t);}
        gateTarget=Math.min(0,gateTarget);
        c.gateBand[b]=.99*c.gateBand[b]+.01*gateTarget;
        v*=.9*this.db2g(c.gateBand[b])+.1;
        const depth=degree/100,downRatio=1+depth*((b===3?100:66.7)-1),upRatio=1+depth*3;
        const downDb=this.g2db(Math.abs(v)+1e-9),downThr=s.udmbc.compT[b],downSlope=1-1/downRatio;
        const downTarget=downDb>downThr?(downDb-downThr)*downSlope:0;
        const bandBaseAttackMs=Math.max(.1,Math.min(120,Number(s.udmbc.compA[b]||0))),k=Math.max(0,(120-bandBaseAttackMs)/.49);
        const dynamicAttackMs=bandBaseAttackMs+k*(depth*depth),minAttackLimit=b===0?15:(b===1?8:1),finalAttackMs=Math.max(minAttackLimit,dynamicAttackMs);
        const baseReleaseMs=Math.max(10,Math.min(2500,Number(s.udmbc.compR[b]||0))),finalReleaseMs=Math.max(20,baseReleaseMs+depth*100);
        const attackCoef=Math.exp(-1000/(finalAttackMs*sampleRate)),releaseCoef=Math.exp(-1000/(finalReleaseMs*sampleRate));
        const dr=releaseCoef;c.comp[b]=c.comp[b]*(downTarget>c.comp[b]?attackCoef:dr)+(1-(downTarget>c.comp[b]?attackCoef:dr))*downTarget;
        const downMix=this.clamp(s.udmbc.compM[b]/100,0,1);v*=this.db2g(-c.comp[b]*downMix)+(1-downMix);
        const upDb=this.g2db(Math.abs(v)+1e-9),upThr=s.udmbc.liftT[b],upSlope=1-1/upRatio,upTarget=upDb<upThr?(upThr-upDb)*upSlope:0;
        const ua=this.tc(s.udmbc.liftA[b]),ur=this.tc(s.udmbc.liftR[b]),upGain=this.db2g(Math.min(12,Math.max(0,upTarget)));
        c.lift[b]=c.lift[b]*(upGain>1?ua:ur)+(1-(upGain>1?ua:ur))*upGain;
        const upMix=this.clamp(s.udmbc.liftM[b]/100,0,1);v*=c.lift[b]*upMix+(1-upMix);
        v*=this.db2g(this.clamp(s.udmbc.level[b],-24,12));bands[b]=v;
      }
      let sum=bands[0]+bands[1]+bands[2]+bands[3];
      if(s.udmbc.clip)sum=Math.tanh(sum*1.7);
      sum*=this.db2g(this.clamp(s.udmbc.output,-24,24));
      const ceilingDb=-.8,inputDb=this.g2db(Math.max(Math.abs(sum),1e-9)),targetRed=inputDb>ceilingDb?-(inputDb-ceilingDb):0;
      const la=this.tc(.05),lr=this.tc(85),lc=targetRed<c.lim?la:lr;
      c.lim=lc*c.lim+(1-lc)*targetRed;sum*=this.db2g(c.lim);
      const mix=this.clamp(s.udmbc.mix/100,0,1);
      y=original*(1-mix)+sum*mix;
    }
    if(!s.type.bypass){
      const ti=y*this.db2g(s.type.input);
      const mix=this.clamp(Number(s.type.mix)/100,0,1);

      // TAPE COLOR is intentionally stateless. Attack / Release and envelope
      // state are retained only for preset compatibility, not gain movement.
      // These three crossover LP states are signal-splitting state, not dynamic gain state.
      // TAPE follows the same X1/X2/X3 split as UDMBC.
      const xs=s.udmbc.x;
      const bands=this.zoneBands(ti,c,"typeLp",xs);

      const driveParams=[0,0,0,0];
      const makeup=[0,0,0,0];
      const trims=[0,0,0,0];
      for(let b=0;b<4;b++){
        const typeMax=[50,60,70,90][b];
        const limitedDegree=this.clamp(Number(s.type.degree[b]||0),0,typeMax);
        const controlNorm=limitedDegree/Math.max(1,typeMax);
        const depth=this.clamp(controlNorm*.5,0,.5);
        const rawDriveParam=1+1.5*depth;
        const driveParam=Math.max(1,rawDriveParam);
        driveParams[b]=driveParam;
        let makeupDenominator=Math.tanh(driveParam);
        makeupDenominator=Math.max(makeupDenominator,1e-6);
        makeup[b]=1/makeupDenominator;
        trims[b]=this.db2g(this.clamp(Number(s.type.level[b]||0),-6,6));
      }

      let enhancement=0;
      for(let b=0;b<4;b++){
        if(s.bandBypass?.[b]||s.type.bandBypass[b])continue;
        const typeMax=[50,60,70,90][b];
        const limitedDegree=this.clamp(Number(s.type.degree[b]||0),0,typeMax);
        const controlNorm=limitedDegree/Math.max(1,typeMax);
        const depth=this.clamp(controlNorm*.5,0,.5);
        if(depth<=0)continue;
        const driven=Math.tanh(bands[b]*driveParams[b])*makeup[b];
        const processed=driven*trims[b];
        enhancement+=(processed-bands[b])*depth;
      }

      y=(ti+enhancement*mix)*this.db2g(this.clamp(Number(s.type.output||0),-24,12));
    }
    return y;
  }
  process(inputs,outputs){
    const out=outputs[0],inp=inputs[0];
    try{
      if(this.pendingState){
        this.s=this.pendingState;
        this.activeRevision=this.pendingRevision;
        this.pendingState=null;
      }
      if(!this.ready||!this.s||!inp||!inp.length){for(const c of out)c.fill(0);return true;}
    if(!this._sentReady){
      this._sentReady=true;
      this.port.postMessage({type:"ready"});
    }
    const L=inp[0],R=inp[1]||inp[0],stereo=inp.length>1;
    const mix=this.clamp((this.s.mix.bypass?100:this.s.mix.drywet)/100,0,1),og=this.db2g(this.clamp(this.s.mix.output,-24,12));
    const soloBand=Number(this.s.solo?.band??-1),graphSolo=!!this.s.solo?.graphActive,soloEnabled=graphSolo||(soloBand>=0&&soloBand<4),xs=this.s.udmbc.x;
    const analogSmoothingCoeff=Math.exp(-1/(0.001*0.25*sampleRate));
    const analogAlpha=this.analogAlpha;
    const bandProcessingHpCoef=this.hp(30,.7071067811865476);

    for(let b=0;b<4;b++){
      const active=!this.s.eq.globalBypass&&!this.s.eq.colorBypass[b]&&Number(this.s.eq.color[b]||0)>1e-6;
      const ss=!!this.s.eq.mode[b];
      const target=active?this.clamp(Number(this.s.eq.color[b]||0),0,60)/100*(ss?1.80:1.55):0;
      if(this.analogMode[b]!==ss){
        this.analogMode[b]=ss;
        this.analogAlpha[b]=target;
        this.analogAlphaInitialized[b]=true;
        this.ch[0].analogAd[b]={prevX:0,hasPrev:false};
        this.ch[1].analogAd[b]={prevX:0,hasPrev:false};
      }else if(!this.analogAlphaInitialized[b]){
        this.analogAlpha[b]=target;
        this.analogAlphaInitialized[b]=true;
      }else if(!active){
        this.analogAlpha[b]=0;
        this.ch[0].analogAd[b]={prevX:0,hasPrev:false};
        this.ch[1].analogAd[b]={prevX:0,hasPrev:false};
      }
    }

    for(let n=0;n<out[0].length;n++){
      for(let b=0;b<4;b++){
        const active=!this.s.eq.globalBypass&&!this.s.eq.colorBypass[b]&&Number(this.s.eq.color[b]||0)>1e-6;
        if(active){
          const ss=!!this.s.eq.mode[b];
          const target=this.clamp(Number(this.s.eq.color[b]||0),0,60)/100*(ss?1.80:1.55);
          this.analogAlpha[b]+=(target-this.analogAlpha[b])*(1-analogSmoothingCoeff);
        }
      }
      const l=L[n]||0,r=R[n]||0;
      const dyn=this.dynamicStereo(l,r,stereo);

      // One shared audible 30 Hz floor, then base-rate TRANSIENT, then Analog.
      const floorL=this.biquad(dyn[0],bandProcessingHpCoef,this.ch[0].bandProcessingHp);
      const floorR=stereo
        ? this.biquad(dyn[1],bandProcessingHpCoef,this.ch[1].bandProcessingHp)
        : dyn[1];
      const transientOut=this.applyTransientStereo(floorL,floorR,stereo,xs);
      const moduleL=this.sample(transientOut[0],0,analogAlpha);
      const moduleR=this.sample(transientOut[1],1,analogAlpha);

      let yL=moduleL;
      let yR=moduleR;
      yL=(l+mix*(yL-l))*og;yR=(r+mix*(yR-r))*og;
      if(this.s.solo && Number(this.s.solo.band)!==this._lastSoloBand){this.soloBlend=0;this._lastSoloBand=Number(this.s.solo.band)}
      if(this.s.solo && !!this.s.solo.post!==this._lastSoloPost){this.soloBlend=0;this._lastSoloPost=!!this.s.solo.post}
      const targetSolo=soloEnabled?1:0;
      if(this.soloBlend<targetSolo)this.soloBlend=Math.min(targetSolo,this.soloBlend+1/64);else if(this.soloBlend>targetSolo)this.soloBlend=Math.max(targetSolo,this.soloBlend-1/64);
      if(this.soloBlend>0){
        let preL,preR,postL,postR;
        if(graphSolo){
          const gf=this.clamp(Number(this.s.solo?.graphFreq??1000),20,sampleRate*.45);
          const gq=this.clamp(Number(this.s.solo?.graphQ??.707),.1,18);
          const gc=this.bp(gf,gq);
          preL=this.biquad(l,gc,this.ch[0].graphSoloPre);preR=this.biquad(r,gc,this.ch[1].graphSoloPre);
          postL=this.biquad(yL,gc,this.ch[0].graphSoloPost);postR=this.biquad(yR,gc,this.ch[1].graphSoloPost);
        }else{
          preL=this.zoneBands(l,this.ch[0],"soloPre",xs)[soloBand];preR=this.zoneBands(r,this.ch[1],"soloPre",xs)[soloBand];
          postL=this.zoneBands(yL,this.ch[0],"soloPost",xs)[soloBand];postR=this.zoneBands(yR,this.ch[1],"soloPost",xs)[soloBand];
        }
        const soloL=this.s.solo.post?postL:preL,soloR=this.s.solo.post?postR:preR;
        yL=yL*(1-this.soloBlend)+soloL*this.soloBlend;yR=yR*(1-this.soloBlend)+soloR*this.soloBlend;
      }
      const target=this.s.masterBypass?1:0,step=1/64;
      if(this.masterBlend<target)this.masterBlend=Math.min(target,this.masterBlend+step);else if(this.masterBlend>target)this.masterBlend=Math.max(target,this.masterBlend-step);
      yL=yL*(1-this.masterBlend)+l*this.masterBlend;yR=yR*(1-this.masterBlend)+r*this.masterBlend;
      if(this.s.delta){yL=yL-l;yR=yR-r;}
      out[0][n]=this.finite(yL);if(out[1])out[1][n]=this.finite(yR);
    }
    // UI meters are diagnostic only; ~10 Hz is enough and keeps MessagePort
    // traffic well below the parameter-update rate.
    this._meterBlocks++;
    if(this._meterBlocks%32===0){
      this.port.postMessage({
        type:"dynMeters",
        revision:this.activeRevision,
        mid:(this.s.dyn.gainMid||[0,0,0,0]).slice(),
        side:(this.s.dyn.gainSide||[0,0,0,0]).slice()
      });
    }
    return true;
    }catch(err){
      // A DSP exception must never terminate the audio graph. Pass the current
      // input through for this quantum and let the UI route around the failed
      // processor.
      if(out?.[0]){
        const inL=inp?.[0],inR=inp?.[1]||inL;
        for(let n=0;n<out[0].length;n++){
          out[0][n]=this.finite(inL?.[n]||0);
          if(out[1])out[1][n]=this.finite(inR?.[n]||0);
        }
      }
      if(!this._errorReported){
        this._errorReported=true;
        try{
          this.port.postMessage({
            type:"error",
            revision:this.activeRevision,
            message:String(err?.message||err)
          });
        }catch{}
      }
      return true;
    }
  }
}
registerProcessor("vvchain-worklet",VVChainWorklet);
