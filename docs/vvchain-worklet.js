// VVChain Web AudioWorklet DSP module · v1.0.16
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
    this.pendingRevision=0;
    this.activeRevision=0;
    this._errorReported=false;
    this.ch=[this.makeCh(),this.makeCh()];
    this.port.onmessage=e=>{
      if(!e.data||e.data.type!=="params")return;
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
    const dynState=()=>({det:{z1:0,z2:0},eq:{g:0,k:1,a1:1,a2:0,a3:0,m1:0,ic1:0,ic2:0},env:-120});
    return {
      eq:Array.from({length:4},()=>({g:0,k:1,a1:1,a2:0,a3:0,m1:0,ic1:0,ic2:0})),
      dynMid:Array.from({length:4},dynState),
      dynSide:Array.from({length:4},dynState),
      analogPrev:[0,0,0,0], analogDc:[0,0,0,0], analogPower:[0,0,0,0],
      analogLp:[0,0,0], lp:[0,0,0], typeLp:[0,0,0], gate:0, gateBand:[0,0,0,0], lim:0,
      lift:[1,1,1,1], comp:[0,0,0,0], typeFast:[0,0,0,0], typeSlow:[0,0,0,0], typeDc:[0,0,0,0],
      deHp:{z1:0,z2:0}, deHp2:{z1:0,z2:0}, deFast:0, deSlow:0, deGain:0, soloPre:[0,0,0], soloPost:[0,0,0], graphSoloPre:{z1:0,z2:0}, graphSoloPost:{z1:0,z2:0}
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
  hp(f,q=.707){
    const w=2*Math.PI*this.clamp(f,10,sampleRate*.45)/sampleRate;
    const a=Math.sin(w)/(2*Math.max(.1,q)),cc=Math.cos(w);
    const b0=(1+cc)/2,b1=-(1+cc),b2=(1+cc)/2;
    const a0=1+a,a1=-2*cc,a2=1-a;
    return[b0/a0,b1/a0,b2/a0,a1/a0,a2/a0]
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
      const bpCoef=this.bp(f,baseQ);
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
      // Cytomic / Simper TPT Bell. Q is passed directly because the Bell
      // denominator already uses k = 1 / (Q * A).
      mid=this.tptBell(mid,md.eq,sampleRate,f,baseQ,mDynamicGain);
      if(stereo)side=this.tptBell(side,sd.eq,sampleRate,f,baseQ,sDynamicGain);
    }
    if(stereo)return[(mid+side)*invSqrt2,(mid-side)*invSqrt2];
    return[mid,r];
  }
  // v1.0.16 unity-normalized smooth zero-phase algebraic saturation.
  analog(x,a,ss,ch,b,x2=1){
    a=this.clamp(a,0,1);
    ch.analogPrev[b]=x; ch.analogDc[b]=0; ch.analogPower[b]=0;
    if(a<=1e-6)return x;
    const modeAlpha=ss?1.80:1.55;
    const alpha=a*modeAlpha;
    const unityNorm=Math.pow(1+alpha,.25);
    const u=this.clamp(x,-1,1);
    const denominator=Math.sqrt(Math.sqrt(1+alpha*u*u));
    const saturated=(u/denominator)*unityNorm;
    const protectedSaturated=Math.sign(u||1)*Math.max(Math.abs(saturated),Math.abs(u));
    return x+(protectedSaturated-u)*this.clamp(x2,1,1.6);
  }
  deessSample(x,c,coef){
    const st=this.s.de;
    if(st.bypass||Number(st.intensity||0)<=0)return x;
    const high1=this.biquad(x,coef,c.deHp),high=this.biquad(high1,coef,c.deHp2);
    const low=x-high,sc=Math.abs(high);
    const modes=[
      {attack:5,release:120,ratio:3},
      {attack:2,release:70,ratio:4},
      {attack:.75,release:35,ratio:8},
      {attack:.25,release:20,ratio:10}
    ];
    const mode=Math.max(0,Math.min(3,Math.round(Number(st.mode||2))-1));
    const preset=modes[mode];
    const fa=sc>c.deFast?this.tc(preset.attack):this.tc(preset.release);
    c.deFast=fa*c.deFast+(1-fa)*sc;
    const sa=sc>c.deSlow?this.tc(Math.max(20,preset.attack*10)):this.tc(Math.max(80,preset.release*4));
    c.deSlow=sa*c.deSlow+(1-sa)*sc;
    const excess=this.g2db(Math.max(c.deFast,1e-9))-this.g2db(Math.max(c.deSlow,1e-9))-2-Number(st.offset||0);
    const ratioShape=1-1/Math.max(1.1,preset.ratio);
    const red=this.clamp(
      Number(st.intensity||0)*this.clamp((excess/6)*ratioShape,0,1),0,8);
    const ga=red>c.deGain?this.tc(preset.attack):this.tc(preset.release);
    c.deGain=ga*c.deGain+(1-ga)*red;
    return low+high*this.db2g(-c.deGain);
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
  sample(x,ch){
    const s=this.s,c=this.ch[ch];let y=x;
    if(!s.eq.bypass){
      for(let b=0;b<4;b++){
        if(s.bandBypass?.[b])continue;
        y=this.tptBell(y,c.eq[b],sampleRate,s.eq.freq[b],s.eq.q[b],s.eq.gain[b]);
      }
    }
    // ANALOG COLOR v1.0.16: unity-normalized smooth saturation.
    // X2 preserves its established role: it multiplies only the generated ANALOG delta.
    if(!s.eq.globalBypass){
      for(let b=0;b<4;b++){
        if(s.eq.colorBypass[b])continue;
        const amount=this.clamp(Number(s.eq.color[b]||0)/100,0,1);
        if(amount<=1e-6)continue;
        const x2=s.eq.colorX2?.[b]?1.6:1;
        y=this.analog(y,amount,!!s.eq.mode[b],c,b,x2);
      }
    }
    if(!s.ott.bypass){
      const original=y,inputGain=this.db2g(this.clamp(s.ott.input,-24,24)),xs=s.ott.x,z=original*inputGain;
      c.lp[0]+=(1-Math.exp(-2*Math.PI*xs[0]/sampleRate))*(z-c.lp[0]);const h0=z-c.lp[0];
      c.lp[1]+=(1-Math.exp(-2*Math.PI*xs[1]/sampleRate))*(h0-c.lp[1]);const h1=h0-c.lp[1];
      c.lp[2]+=(1-Math.exp(-2*Math.PI*xs[2]/sampleRate))*(h1-c.lp[2]);
      const bands=[c.lp[0],c.lp[1],c.lp[2],h1-c.lp[2]];
      for(let b=0;b<4;b++){
        if(s.bandBypass?.[b]||s.ott.bandBypass[b])continue;
        const degree=this.clamp(Number(s.ott.degree[b]||0),0,100);
        if(degree<=1e-4)continue;
        let v=bands[b];
        const gateDb=this.g2db(Math.abs(v)+1e-9),gt=s.ott.gate,knee=9,slope=5;
        const kneeStart=gt-knee/2,kneeEnd=gt+knee/2;let gateTarget=0;
        if(gateDb<kneeStart)gateTarget=(gateDb-gt)*slope;
        else if(gateDb<kneeEnd){const t=this.clamp((gateDb-kneeStart)/knee,0,1);gateTarget=(gateDb-gt)*slope*(1-t)*(1-t);}
        gateTarget=Math.min(0,gateTarget);
        c.gateBand[b]=.99*c.gateBand[b]+.01*gateTarget;
        v*=.9*this.db2g(c.gateBand[b])+.1;
        const depth=degree/100,downRatio=1+depth*((b===3?100:66.7)-1),upRatio=1+depth*3;
        const downDb=this.g2db(Math.abs(v)+1e-9),downThr=s.ott.compT[b],downSlope=1-1/downRatio;
        const downTarget=downDb>downThr?(downDb-downThr)*downSlope:0;
        const bandBaseAttackMs=Math.max(.1,Math.min(120,Number(s.ott.compA[b]||0))),k=Math.max(0,(120-bandBaseAttackMs)/.49);
        const dynamicAttackMs=bandBaseAttackMs+k*(depth*depth),minAttackLimit=b===0?15:(b===1?8:1),finalAttackMs=Math.max(minAttackLimit,dynamicAttackMs);
        const baseReleaseMs=Math.max(10,Math.min(2500,Number(s.ott.compR[b]||0))),finalReleaseMs=Math.max(20,baseReleaseMs+depth*100);
        const attackCoef=Math.exp(-1000/(finalAttackMs*sampleRate)),releaseCoef=Math.exp(-1000/(finalReleaseMs*sampleRate));
        const dr=releaseCoef;c.comp[b]=c.comp[b]*(downTarget>c.comp[b]?attackCoef:dr)+(1-(downTarget>c.comp[b]?attackCoef:dr))*downTarget;
        const downMix=this.clamp(s.ott.compM[b]/100,0,1);v*=this.db2g(-c.comp[b]*downMix)+(1-downMix);
        const upDb=this.g2db(Math.abs(v)+1e-9),upThr=s.ott.liftT[b],upSlope=1-1/upRatio,upTarget=upDb<upThr?(upThr-upDb)*upSlope:0;
        const ua=this.tc(s.ott.liftA[b]),ur=this.tc(s.ott.liftR[b]),upGain=this.db2g(Math.min(12,Math.max(0,upTarget)));
        c.lift[b]=c.lift[b]*(upGain>1?ua:ur)+(1-(upGain>1?ua:ur))*upGain;
        const upMix=this.clamp(s.ott.liftM[b]/100,0,1);v*=c.lift[b]*upMix+(1-upMix);
        v*=this.db2g(this.clamp(s.ott.level[b],-24,12));bands[b]=v;
      }
      let sum=bands[0]+bands[1]+bands[2]+bands[3];
      if(s.ott.clip)sum=Math.tanh(sum*1.7);
      sum*=this.db2g(this.clamp(s.ott.output,-24,24));
      const ceilingDb=-.8,inputDb=this.g2db(Math.max(Math.abs(sum),1e-9)),targetRed=inputDb>ceilingDb?-(inputDb-ceilingDb):0;
      const la=this.tc(.05),lr=this.tc(85),lc=targetRed<c.lim?la:lr;
      c.lim=lc*c.lim+(1-lc)*targetRed;sum*=this.db2g(c.lim);
      const mix=this.clamp(s.ott.mix/100,0,1);
      y=original*(1-mix)+sum*mix;
    }
    if(!s.type.bypass){
      const ti=y*this.db2g(s.type.input);
      const mix=this.clamp(Number(s.type.mix)/100,0,1);

      // TAPE-A is intentionally stateless. Attack / Release and envelope
      // state are retained only for preset compatibility, not gain movement.
      // These three crossover LP states are signal-splitting state, not dynamic gain state.
      // TYPE-A follows the same X1/X2/X3 split as OTT.
      const xs=s.ott.x;
      const bands=this.zoneBands(ti,c,"typeLp",xs);

      const driveParams=[0,0,0,0];
      const makeup=[0,0,0,0];
      const trims=[0,0,0,0];
      for(let b=0;b<4;b++){
        const typeMax=[50,60,70,90][b];
        const limitedDegree=this.clamp(Number(s.type.degree[b]||0),0,typeMax);
        const depth=this.clamp(limitedDegree/100,0,1);
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
        const depth=this.clamp(limitedDegree/100,0,1);
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
    const deCoef=this.hp(this.clamp(Number(this.s.de.freq||8000),6000,18000));
    const mix=this.clamp((this.s.mix.bypass?100:this.s.mix.drywet)/100,0,1),og=this.db2g(this.clamp(this.s.mix.output,-24,12));
    const soloBand=Number(this.s.solo?.band??-1),graphSolo=!!this.s.solo?.graphActive,soloEnabled=graphSolo||(soloBand>=0&&soloBand<4),xs=this.s.ott.x;
    for(let n=0;n<out[0].length;n++){
      const l=L[n]||0,r=R[n]||0;
      const dyn=this.dynamicStereo(l,r,stereo);
      let yL=this.deessSample(this.sample(dyn[0],0),this.ch[0],deCoef);
      let yR=this.deessSample(this.sample(dyn[1],1),this.ch[1],deCoef);
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
