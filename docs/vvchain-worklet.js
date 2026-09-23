// VVChain Web AudioWorklet DSP module · 2026-09-23 19:17
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
    this.ch=[this.makeCh(),this.makeCh()];
    this.port.onmessage=e=>{
      if(e.data&&e.data.type==="params"){
        const next=e.data.state;
        if(this.s&&(
          Number(this.s.solo?.band??-1)!==Number(next.solo?.band??-1) ||
          !!this.s.solo?.post!==!!next.solo?.post))
          this.soloBlend=0;
        this.s=next;
        this.ready=true;
      }
    };
  }
  makeCh(){
    const dynState=()=>({det:{z1:0,z2:0},eq:{z1:0,z2:0},env:-120});
    return {
      eq:Array.from({length:4},()=>({z1:0,z2:0})),
      dynMid:Array.from({length:4},dynState),
      dynSide:Array.from({length:4},dynState),
      analogPrev:[0,0,0,0], analogDc:[0,0,0,0], analogPower:[0,0,0,0],
      lp:[0,0,0], typeLp:[0,0,0], gate:0, gateBand:[0,0,0,0], lim:0,
      lift:[1,1,1,1], comp:[0,0,0,0], typeFast:[0,0,0,0], typeSlow:[0,0,0,0], typeDc:[0,0,0,0],
      deHp:{z1:0,z2:0}, deHp2:{z1:0,z2:0}, deFast:0, deSlow:0, deGain:0, soloPre:[0,0,0], soloPost:[0,0,0]
    };
  }
  clamp(v,a,b){return Math.max(a,Math.min(b,v))}
  finite(v){return Number.isFinite(v)?v:0}
  db2g(db){return Math.pow(10,db/20)}
  g2db(g){return 20*Math.log10(Math.max(g,1e-9))}
  tc(ms){return Math.exp(-1/(.001*Math.max(.1,ms)*sampleRate))}
  biquad(x,c,z){const y=c[0]*x+z.z1;z.z1=c[1]*x-c[3]*y+z.z2;z.z2=c[2]*x-c[4]*y;return y}
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
      if(s.dyn.detectOnsets?.[b]){
        midLevel+=this.clamp(Math.max(0,mDb-md.slow)*1.5,0,12);
        sideLevel+=this.clamp(Math.max(0,sDb-sd.slow)*1.5,0,12);
      }

      const ma=activation(midLevel,!!s.dyn.triggerBelow?.[b]);
      const sa=activation(sideLevel,!!s.dyn.triggerBelow?.[b]);
      const mtc=ma>(md.activation??0)?ac:rc;
      const stc=sa>(sd.activation??0)?ac:rc;
      md.activation=mtc*(md.activation??0)+(1-mtc)*ma;
      sd.activation=stc*(sd.activation??0)+(1-stc)*sa;

      const delta=dynamicsDirection*dynamicRangeDb;
      const mChange=delta*md.activation*mw;
      const sChange=delta*sd.activation*sw;
      s.dyn.gainMid[b]=.90*Number(s.dyn.gainMid[b]||0)+.10*mChange;
      s.dyn.gainSide[b]=.90*Number(s.dyn.gainSide[b]||0)+.10*sChange;

      const mGain=this.clamp(offset+s.dyn.gainMid[b],-36,36);
      const sGain=this.clamp(offset+s.dyn.gainSide[b],-36,36);
      const mq=this.clamp(baseQ/(1+.045*Math.abs(mGain)),.1,18);
      const sq=this.clamp(baseQ/(1+.045*Math.abs(sGain)),.1,18);
      const mCoef=this.peak(sampleRate,f,mq,mGain);
      const sCoef=this.peak(sampleRate,f,sq,sGain);

      md.eq.z1=md.eq.z1||0;sd.eq.z1=sd.eq.z1||0;
      mid=this.biquad(mid,mCoef,md.eq);
      if(stereo)side=this.biquad(side,sCoef,sd.eq);
    }
    if(stereo)return[(mid+side)*invSqrt2,(mid-side)*invSqrt2];
    return[mid,r];
  }
  analog(x,a,ss,ch,b){
    a=this.clamp(a,0,1);
    ch.analogPrev[b]=x; ch.analogDc[b]=0; ch.analogPower[b]=0;
    if(a<=0)return x;
    const u=this.clamp(x,-1,1),u2=u*u;
    const t3=4*u*u2-3*u,u5=u*u2*u2;
    const t5=16*u*u2*u2-20*u*u2+5*u;
    const h3=ss?0.020:0.014,h5=ss?0.006:0.004;
    const shaped=u+a*(h3*(t3-u)+h5*(t5-u));
    return x+0.90*(shaped-u);
  }
  deessSample(x,c,coef){
    const st=this.s.de;
    if(st.bypass||Number(st.intensity||0)<=0)return x;
    const high1=this.biquad(x,coef,c.deHp),high=this.biquad(high1,coef,c.deHp2);
    const low=x-high,sc=Math.abs(high);
    const fa=sc>c.deFast?this.tc(.25):this.tc(45);
    c.deFast=fa*c.deFast+(1-fa)*sc;
    const sa=sc>c.deSlow?this.tc(75):this.tc(260);
    c.deSlow=sa*c.deSlow+(1-sa)*sc;
    const excess=this.g2db(Math.max(c.deFast,1e-9))-this.g2db(Math.max(c.deSlow,1e-9))-2-Number(st.offset||0);
    const red=this.clamp(Number(st.intensity||0)*this.clamp(excess/6,0,1),0,8);
    const ga=red>c.deGain?this.tc(.35):this.tc(60);
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
        y=this.biquad(y,this.peak(sampleRate,s.eq.freq[b],s.eq.q[b],s.eq.gain[b]),c.eq[b]);
        if(!s.eq.globalBypass&&!s.eq.colorBypass[b])y=this.analog(y,Number(s.eq.color[b]||0)/100,!!s.eq.mode[b],c,b);
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
      const ti=y*this.db2g(s.type.input),attack=this.tc(s.type.attack),release=this.tc(s.type.release);
      const a80=1-Math.exp(-2*Math.PI*80/sampleRate),a3k=1-Math.exp(-2*Math.PI*3000/sampleRate),a9k=1-Math.exp(-2*Math.PI*9000/sampleRate);
      c.typeLp[0]+=a80*(ti-c.typeLp[0]);const b1=c.typeLp[0];
      c.typeLp[1]+=a3k*(ti-c.typeLp[1]);const b3=ti-c.typeLp[1];
      c.typeLp[2]+=a9k*(ti-c.typeLp[2]);const b4=ti-c.typeLp[2];
      const bands=[b1,ti-b1-b3,b3,b4];let enhancement=0;
      for(let b=0;b<4;b++){
        if(s.bandBypass?.[b]||s.type.bandBypass[b])continue;
        const degree=this.clamp(Number(s.type.degree[b]||0),0,100);if(degree<=0)continue;
        const mag=Math.abs(bands[b]),fastA=mag>c.typeFast[b]?attack:release;
        c.typeFast[b]=fastA*c.typeFast[b]+(1-fastA)*mag;
        const slowA=mag>c.typeSlow[b]?attack:release;
        c.typeSlow[b]=slowA*c.typeSlow[b]+(1-slowA)*mag;
        const levelDb=this.g2db(Math.max(c.typeSlow[b],1e-7)),depth=degree/100,threshold=-56+20*Math.sqrt(depth),ratio=1+15*Math.sqrt(depth),slope=1-1/Math.max(1,ratio),kneeStart=threshold-3,kneeEnd=threshold+3;
        let target=0;if(levelDb<kneeStart)target=(threshold-levelDb)*slope;else if(levelDb<kneeEnd){const xk=kneeEnd-levelDb;target=slope/12*xk*xk;}
        target=this.clamp(target*depth,0,9);const ga=target>c.typeDc[b]?attack:release;c.typeDc[b]=ga*c.typeDc[b]+(1-ga)*target;
        const bandTrim=this.db2g(this.clamp(s.type.level[b],-6,6));enhancement+=bands[b]*(this.db2g(c.typeDc[b])*bandTrim-1);
      }
      y=(ti+enhancement*this.clamp(s.type.mix/100,0,1))*this.db2g(this.clamp(s.type.output,-24,12));
    }
    return y;
  }
  process(inputs,outputs){
    const out=outputs[0],inp=inputs[0];
    if(!this.ready||!this.s||!inp||!inp.length){for(const c of out)c.fill(0);return true;}
    if(!this._sentReady){
      this._sentReady=true;
      this.port.postMessage({type:"ready"});
    }
    const L=inp[0],R=inp[1]||inp[0],stereo=inp.length>1;
    const deCoef=this.hp(this.clamp(Number(this.s.de.freq||8000),6000,18000));
    const mix=this.clamp((this.s.mix.bypass?100:this.s.mix.drywet)/100,0,1),og=this.db2g(this.clamp(this.s.mix.output,-24,12));
    const soloBand=Number(this.s.solo?.band??-1),soloEnabled=soloBand>=0&&soloBand<4,xs=this.s.ott.x;
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
        const preL=this.zoneBands(l,this.ch[0],"soloPre",xs)[soloBand],preR=this.zoneBands(r,this.ch[1],"soloPre",xs)[soloBand];
        const postL=this.zoneBands(yL,this.ch[0],"soloPost",xs)[soloBand],postR=this.zoneBands(yR,this.ch[1],"soloPost",xs)[soloBand];
        const soloL=this.s.solo.post?postL:preL,soloR=this.s.solo.post?postR:preR;
        yL=yL*(1-this.soloBlend)+soloL*this.soloBlend;yR=yR*(1-this.soloBlend)+soloR*this.soloBlend;
      }
      const target=this.s.masterBypass?1:0,step=1/64;
      if(this.masterBlend<target)this.masterBlend=Math.min(target,this.masterBlend+step);else if(this.masterBlend>target)this.masterBlend=Math.max(target,this.masterBlend-step);
      yL=yL*(1-this.masterBlend)+l*this.masterBlend;yR=yR*(1-this.masterBlend)+r*this.masterBlend;
      if(s.delta){yL=yL-l;yR=yR-r;}
      out[0][n]=this.finite(yL);if(out[1])out[1][n]=this.finite(yR);
    }
    this._meterBlocks++;
    if(this._meterBlocks%8===0){
      this.port.postMessage({type:"dynMeters",mid:this.s.dyn.gainMid,side:this.s.dyn.gainSide});
    }
    return true;
  }
}
registerProcessor("vvchain-worklet",VVChainWorklet);
