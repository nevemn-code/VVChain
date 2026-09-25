// VVChain Web AudioWorklet DSP module · v1.0.65
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
    this.udLinked=Array.from({length:4},()=>({
      gateEnvDb:0,lifterEnv:1,compEnvDb:0,
      upRmsPower:0,upSlowRmsPower:0,downRmsPower:0,downSlowRmsPower:0
    }));
    this.udmbcStateLive=false;
    this.typeStateLive=false;
    this._xcoBase=null;
    this._xcoType=null;
    this._udCache=null;
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
    const bq=()=>({z1:0,z2:0});
    const xo=()=>({lp1:bq(),lp2:bq(),hp1:bq(),hp2:bq()});
    const tree=()=>[xo(),xo(),xo()];
    return {
      eq:Array.from({length:4},eqFilter),
      dynMid:Array.from({length:4},dynState),
      dynSide:Array.from({length:4},dynState),
      analogAd:Array.from({length:4},()=>({prevX:0,hasPrev:false})),
      typeAd:Array.from({length:4},()=>({prevX:0,hasPrev:false})),
      analogLp:tree(),
      lp:tree(),
      udPhase2B1:xo(),udPhase3B1:xo(),udPhase3B2:xo(),
      typeLp:tree(),
      transientLp:tree(),
      soloPre:tree(),
      soloPost:tree(),
      gate:0,lim:0,
      transientHp:bq(),
      graphSoloPre:bq(),graphSoloPost:bq()
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

  xoverQ(){
    const ov=Number(this.s?.udmbc?.overlap?.[0]??50);
    return .90-.35*this.clamp(ov,0,100)/100;
  }
  xoverCoefs(xs,firstMin=80,firstMax=900){
    const x1=this.clamp(Number(xs?.[0]??90),firstMin,firstMax);
    const x2=this.clamp(Number(xs?.[1]??2500),x1+80,5000);
    const x3=this.clamp(Number(xs?.[2]??7000),x2+200,sampleRate*.42);
    const q=this.xoverQ();
    return [x1,x2,x3].map(f=>({lp:this.lp(f,q),hp:this.hp(f,q)}));
  }
  resetBq(z){z.z1=0;z.z2=0}
  resetXover(z){
    this.resetBq(z.lp1);this.resetBq(z.lp2);
    this.resetBq(z.hp1);this.resetBq(z.hp2);
  }
  resetTree(tree){for(const z of tree)this.resetXover(z)}
  xoverPair(x,z,coef){
    const low=this.biquad(this.biquad(x,coef.lp,z.lp1),coef.lp,z.lp2);
    const high=this.biquad(this.biquad(x,coef.hp,z.hp1),coef.hp,z.hp2);
    return[low,high];
  }
  xoverAllPass(x,z,coef){
    const p=this.xoverPair(x,z,coef);
    return p[0]+p[1];
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
  zoneBands(x,c,which,xs,coefs=this._xcoBase){
    const tree=c[which];
    const cs=coefs||this.xoverCoefs(xs,80,900);
    const p1=this.xoverPair(x,tree[0],cs[0]);
    const p2=this.xoverPair(p1[1],tree[1],cs[1]);
    const p3=this.xoverPair(p2[1],tree[2],cs[2]);
    return[p1[0],p2[0],p3[0],p3[1]];
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

  analogStage(x,chIndex,analogAlpha){
    const s=this.s,c=this.ch[chIndex];
    const bands=this.zoneBands(x,c,"analogLp",s.udmbc.x,this._xcoBase);
    let delta=0;
    for(let b=0;b<4;b++){
      const bandInput=bands[b];
      if(s.eq.globalBypass||s.eq.colorBypass[b]||analogAlpha[b]<=1e-6)continue;
      const x2=s.eq.colorX2?.[b]?2:1;
      delta+=this.analog(bandInput,analogAlpha[b],c,b,x2)-bandInput;
    }
    return x+delta;
  }

  resetUdmbcState(){
    for(const c of this.ch){
      this.resetTree(c.lp);
      this.resetXover(c.udPhase2B1);
      this.resetXover(c.udPhase3B1);
      this.resetXover(c.udPhase3B2);
    }
    this.udLinked=Array.from({length:4},()=>({
      gateEnvDb:0,lifterEnv:1,compEnvDb:0,
      upRmsPower:0,upSlowRmsPower:0,downRmsPower:0,downSlowRmsPower:0
    }));
    this.udmbcStateLive=false;
  }
  buildUdmbcCache(){
    const s=this.s,active=[false,false,false,false];
    let any=false,amountSum=0,amountCount=0;
    const band=[];
    for(let b=0;b<4;b++){
      active[b]=!s.bandBypass?.[b]&&!s.udmbc.bandBypass[b]&&Number(s.udmbc.degree[b]||0)>1e-4;
      any=any||active[b];
      if(!s.udmbc.bandBypass[b]){
        amountSum+=this.clamp(Number(s.udmbc.degree[b]||0),0,100)/100;
        amountCount++;
      }
      const depth=this.clamp(Number(s.udmbc.degree[b]||0),0,100)/100;
      const baseAttack=this.clamp(Number(s.udmbc.compA[b]||0),.1,120);
      const k=Math.max(0,(120-baseAttack)/.49);
      const finalAttack=Math.max(b===0?15:(b===1?8:1),baseAttack+k*depth*depth);
      const baseRelease=this.clamp(Number(s.udmbc.compR[b]||0),10,2500);
      const finalRelease=Math.max(20,baseRelease+depth*100);
      const upAttack=Number(s.udmbc.liftA[b]||1),upRelease=Number(s.udmbc.liftR[b]||50);
      band.push({
        depth,
        downRatio:1+depth*((b===3?100:66.7)-1),
        upRatio:1+depth*3,
        compMix:this.clamp(Number(s.udmbc.compM[b]||0)/100,0,1),
        lifterMix:this.clamp(Number(s.udmbc.liftM[b]||0)/100,0,1),
        bandGain:this.db2g(this.clamp(Number(s.udmbc.level[b]||0),-24,12)),
        finalAttack,finalRelease,upAttack,upRelease,
        compRelease:this.tc(finalRelease),
        downFastAttack:this.tc(finalAttack),
        downFastRelease:this.tc(finalRelease),
        downSlowAttack:this.tc(Math.max(finalAttack*4,5)),
        downSlowRelease:this.tc(Math.max(finalRelease*1.75,20)),
        upFastAttack:this.tc(upAttack),
        upFastRelease:this.tc(Math.max(.5,upRelease*.35)),
        upSlowAttack:this.tc(Math.max(upAttack*4,5)),
        upSlowRelease:this.tc(Math.max(upRelease*1.75,20))
      });
    }
    return{
      active,any,
      globalActive:Math.abs(Number(s.udmbc.input||0))>1e-4||
                   Math.abs(Number(s.udmbc.output||0))>1e-4||!!s.udmbc.clip,
      mix:this.clamp(Number(s.udmbc.mix||0)/100,0,1),
      inputGain:this.db2g(this.clamp(Number(s.udmbc.input||0),-24,24)),
      outputGain:this.db2g(this.clamp(Number(s.udmbc.output||0),-24,24)),
      autoTrim:this.db2g(-2.5*(amountCount?amountSum/amountCount:0)),
      gateAttack:this.tc(100),gateRelease:this.tc(30),
      band
    };
  }
  rmsPdr(input,st,fastKey,slowKey,attackMs,releaseMs,fa,fr,sa,sr){
    const target=input*input;
    st[fastKey]=(target>st[fastKey]?fa:fr)*st[fastKey]+(1-(target>st[fastKey]?fa:fr))*target;
    st[slowKey]=(target>st[slowKey]?sa:sr)*st[slowKey]+(1-(target>st[slowKey]?sa:sr))*target;
    const fastDb=this.g2db(Math.sqrt(Math.max(st[fastKey],1e-12)));
    const slowDb=this.g2db(Math.sqrt(Math.max(st[slowKey],1e-12)));
    const crest=fastDb-slowDb;
    const blend=this.clamp((crest-1)/8,0,1);
    const programRelease=releaseMs*this.clamp(2-1.8*blend,.2,2);
    const power=st[fastKey]*blend+st[slowKey]*(1-blend);
    return[this.g2db(Math.sqrt(Math.max(power,1e-12))),programRelease];
  }
  gateGain(det,st,thr,cache){
    const knee=9,slope=5,inputDb=this.g2db(Math.max(Math.abs(det),1e-6));
    const ks=thr-knee*.5,ke=thr+knee*.5;
    let target=0;
    if(inputDb<ks)target=(inputDb-thr)*slope;
    else if(inputDb<ke){
      const t=this.clamp((inputDb-ks)/knee,0,1);
      target=(inputDb-thr)*slope*(1-t)*(1-t);
    }
    target=Math.min(0,target);
    const a=target<st.gateEnvDb?cache.gateAttack:cache.gateRelease;
    st.gateEnvDb=a*st.gateEnvDb+(1-a)*target;
    return .90*this.db2g(st.gateEnvDb)+.10;
  }
  compGain(detDb,st,thr,ratio,mix,attack,release){
    const slope=1-1/Math.max(1,ratio),knee=6,ks=thr-knee*.5,ke=thr+knee*.5;
    let target=0;
    if(detDb>ke)target=(detDb-thr)*slope;
    else if(detDb>ks){const x=detDb-ks;target=slope/(2*knee)*x*x;}
    const cur=-st.compEnvDb,a=target>cur?attack:release;
    const sm=a*cur+(1-a)*target;
    st.compEnvDb=-this.clamp(sm,0,60);
    return this.db2g(st.compEnvDb)*mix+(1-mix);
  }
  liftGain(detDb,st,thr,ratio,mix,attackMs,releaseMs,attackCoef){
    const slope=1-1/Math.max(1,ratio),knee=6,ks=thr-knee*.5,ke=thr+knee*.5;
    let target=0;
    if(detDb<ks)target=(thr-detDb)*slope;
    else if(detDb<ke){const x=ke-detDb;target=slope/(2*knee)*x*x;}
    const targetLinear=this.db2g(this.clamp(target,0,12));
    const releaseCoef=this.tc(releaseMs);
    const a=targetLinear>st.lifterEnv?attackCoef:releaseCoef;
    st.lifterEnv=a*st.lifterEnv+(1-a)*targetLinear;
    return st.lifterEnv*mix+(1-mix);
  }
  udmbcStereo(l,r,stereo){
    const s=this.s,cache=this._udCache||this.buildUdmbcCache();
    if(s.udmbc.bypass||cache.mix<=1e-6||(!cache.any&&!cache.globalActive)){
      if(this.udmbcStateLive)this.resetUdmbcState();
      return[l,r];
    }
    if(!cache.any){
      if(this.udmbcStateLive)this.resetUdmbcState();
      const proc=x=>{
        const original=x;let wet=x*cache.inputGain;
        if(s.udmbc.clip)wet=Math.tanh(wet*1.7);
        wet*=cache.outputGain;
        return original+cache.mix*(wet-original);
      };
      return[proc(l),stereo?proc(r):r];
    }
    this.udmbcStateLive=true;

    const originals=[l,r],dry=[[],[]],work=[[],[]];
    const nch=stereo?2:1;
    for(let ch=0;ch<nch;ch++){
      const c=this.ch[ch],x=originals[ch]*cache.inputGain;
      const bands=this.zoneBands(x,c,"lp",s.udmbc.x,this._xcoBase);
      bands[0]=this.xoverAllPass(bands[0],c.udPhase2B1,this._xcoBase[1]);
      bands[0]=this.xoverAllPass(bands[0],c.udPhase3B1,this._xcoBase[2]);
      bands[1]=this.xoverAllPass(bands[1],c.udPhase3B2,this._xcoBase[2]);
      dry[ch]=bands.slice();work[ch]=bands.slice();
    }

    for(let b=0;b<4;b++){
      if(!cache.active[b])continue;
      const st=this.udLinked[b],bc=cache.band[b];
      const linked=stereo
        ?Math.sqrt(.5*(dry[0][b]*dry[0][b]+dry[1][b]*dry[1][b]))
        :Math.abs(dry[0][b]);
      const gg=this.gateGain(linked,st,Number(s.udmbc.gate||-80),cache);
      for(let ch=0;ch<nch;ch++)work[ch][b]*=gg;
      const gated=linked*gg;

      const down=this.rmsPdr(
        gated,st,"downRmsPower","downSlowRmsPower",
        bc.finalAttack,bc.finalRelease,
        bc.downFastAttack,bc.downFastRelease,bc.downSlowAttack,bc.downSlowRelease);
      const cg=this.compGain(
        down[0],st,Number(s.udmbc.compT[b]||-18),
        bc.downRatio,bc.compMix,bc.downFastAttack,bc.compRelease);
      for(let ch=0;ch<nch;ch++)work[ch][b]*=cg;

      const up=this.rmsPdr(
        gated*cg,st,"upRmsPower","upSlowRmsPower",
        bc.upAttack,bc.upRelease,
        bc.upFastAttack,bc.upFastRelease,bc.upSlowAttack,bc.upSlowRelease);
      const lg=this.liftGain(
        up[0],st,Math.max(Number(s.udmbc.liftT[b]||-48),-48),
        bc.upRatio,bc.lifterMix,bc.upAttack,up[1],bc.upFastAttack);
      for(let ch=0;ch<nch;ch++)work[ch][b]*=lg*bc.bandGain;
    }

    const result=[l,r];
    for(let ch=0;ch<nch;ch++){
      let delta=0;
      for(let b=0;b<4;b++)if(cache.active[b])delta+=work[ch][b]-dry[ch][b];
      let wet=originals[ch]*cache.inputGain+delta;
      if(s.udmbc.clip)wet=Math.tanh(wet*1.7);
      wet*=cache.outputGain*cache.autoTrim;
      result[ch]=originals[ch]+cache.mix*(wet-originals[ch]);
    }
    return result;
  }

  typeAAdAA(x,drive,makeup,st){
    const d=Math.max(1,Number(drive||1)),norm=Number(makeup||1);
    const transfer=v=>Math.tanh(d*v)*norm;
    const logCosh=z=>{const a=Math.abs(z);return a+Math.log1p(Math.exp(-2*a))-Math.log(2)};
    const F=v=>logCosh(d*v)*norm/d;
    let y=transfer(x);
    if(st.hasPrev){
      const delta=x-st.prevX;
      y=Math.abs(delta)<1e-7?transfer(.5*(x+st.prevX)):(F(x)-F(st.prevX))/delta;
    }
    st.prevX=x;st.hasPrev=true;
    return this.finite(y)?y:transfer(x);
  }
  resetTypeState(){
    for(const c of this.ch){
      this.resetTree(c.typeLp);
      for(const st of c.typeAd){st.prevX=0;st.hasPrev=false;}
    }
    this.typeStateLive=false;
  }
  typeStage(x,chIndex){
    const s=this.s,c=this.ch[chIndex];
    const mix=this.clamp(Number(s.type.mix||0)/100,0,1);
    const active=[0,1,2,3].map(b=>!s.bandBypass?.[b]&&!s.type.bandBypass[b]&&Number(s.type.degree[b]||0)>1e-4);
    if(s.type.bypass||mix<=1e-6||!active.some(Boolean)){
      if(this.typeStateLive)this.resetTypeState();
      return x;
    }
    this.typeStateLive=true;
    const original=x,ti=x*this.db2g(this.clamp(Number(s.type.input||0),-24,24));
    const bands=this.zoneBands(ti,c,"typeLp",s.udmbc.x,this._xcoType);
    const maxDegree=[50,60,70,90];
    let enhancement=0;
    for(let b=0;b<4;b++){
      if(!active[b]){c.typeAd[b].prevX=0;c.typeAd[b].hasPrev=false;continue;}
      const limited=this.clamp(Number(s.type.degree[b]||0),0,maxDegree[b]);
      const depth=this.clamp((limited/Math.max(1,maxDegree[b]))*.5,0,.5);
      const drive=Math.max(1,1+1.5*depth);
      const makeup=1/Math.max(Math.tanh(drive),1e-6);
      const driven=this.typeAAdAA(bands[b],drive,makeup,c.typeAd[b]);
      const processed=driven*this.db2g(this.clamp(Number(s.type.level[b]||0),-6,6));
      enhancement+=(processed-bands[b])*depth;
    }
    const wet=(ti+enhancement)*this.db2g(this.clamp(Number(s.type.output||0),-24,24));
    return original+mix*(wet-original);
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
    this._xcoBase=this.xoverCoefs(xs,80,900);
    this._xcoType=this.xoverCoefs(xs,40,1000);
    this._udCache=this.buildUdmbcCache();
    const analogSmoothingCoeff=Math.exp(-1/(0.001*0.25*sampleRate));
    const analogAlpha=this.analogAlpha;

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

      // No hidden audible HPF: neutral user settings remain transparent.
      const transientOut=this.applyTransientStereo(dyn[0],dyn[1],stereo,xs);
      let yL=this.analogStage(transientOut[0],0,analogAlpha);
      let yR=stereo?this.analogStage(transientOut[1],1,analogAlpha):transientOut[1];
      const ud=this.udmbcStereo(yL,yR,stereo);
      yL=this.typeStage(ud[0],0);
      yR=stereo?this.typeStage(ud[1],1):ud[1];
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
          preL=this.zoneBands(l,this.ch[0],"soloPre",xs,this._xcoType)[soloBand];preR=this.zoneBands(r,this.ch[1],"soloPre",xs,this._xcoType)[soloBand];
          postL=this.zoneBands(yL,this.ch[0],"soloPost",xs,this._xcoType)[soloBand];postR=this.zoneBands(yR,this.ch[1],"soloPost",xs,this._xcoType)[soloBand];
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
