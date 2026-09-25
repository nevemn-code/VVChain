#!/usr/bin/env python3
from pathlib import Path
import math, json
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/assets/ui/png'; K=OUT/'knobs'; B=OUT/'backgrounds'; C=OUT/'components'; BT=C/'buttons'; L=C/'leds'; S=C/'sliders'; SC=C/'screws'
for p in [K,B,BT,L,S,SC]: p.mkdir(parents=True,exist_ok=True)
AM=(255,133,34); AMH=(255,197,103); PU=(119,55,255); PUH=(184,120,255); CH=(219,188,139); SEED=862606

def R(c,a=255): return tuple(c)+(a,)
def mix(a,b,t): return tuple(round(a[i]*(1-t)+b[i]*t) for i in range(3))
def ss(img,size): return img.resize(size,Image.Resampling.LANCZOS)

def panel():
    w,h=1500,930; rng=np.random.default_rng(SEED); y=np.linspace(0,1,h)[:,None]; x=np.linspace(0,1,w)[None,:]
    base=19+12*np.exp(-((x-.54)/.33)**2)+3*np.cos((y-.2)*math.pi); row=rng.normal(0,2,(h,1)); fine=rng.normal(0,1.7,(h,w))
    lum=base+row+fine*.28; rgb=np.stack([lum*.96,lum*.98,lum*1.03],2)
    lp=np.exp(-((x-.02)/.15)**2); la=np.exp(-((x-.98)/.16)**2)
    rgb[:,:,0]+=lp*7+la*12; rgb[:,:,1]+=lp*1+la*5; rgb[:,:,2]+=lp*16
    vig=1-.10*np.minimum(1,((x-.5)/.5)**2+((y-.5)/.5)**2); rgb*=vig[:,:,None]
    return Image.fromarray(np.clip(rgb,0,255).astype('uint8'),'RGB')

def knob(angle,cell=192):
    q=4; z=cell*q; im=Image.new('RGBA',(z,z),(0,0,0,0)); d=ImageDraw.Draw(im); c=z//2
    d.ellipse((24*q,31*q,168*q,175*q),fill=(0,0,0,95)); d.ellipse((23*q,20*q,169*q,166*q),fill=(12,12,15,255),outline=R(CH,90),width=2*q)
    d.ellipse((30*q,27*q,162*q,159*q),fill=(24,24,28,255),outline=R(PU,150),width=2*q)
    for k in range(56):
        a=2*math.pi*k/56; r1=61*q; r2=(64 if k%2 else 65)*q
        d.line((c+math.cos(a)*r1,c+math.sin(a)*r1,c+math.cos(a)*r2,c+math.sin(a)*r2),fill=(120,82,57,125),width=2)
    for k in range(96):
        a0=2*math.pi*k/96; a1=2*math.pi*(k+1)/96; v=int(35+26*(math.cos(a0+2.2)+1)/2+8*math.sin(a0*4)); rr=56*q
        d.polygon([(c,c),(c+math.cos(a0)*rr,c+math.sin(a0)*rr),(c+math.cos(a1)*rr,c+math.sin(a1)*rr)],fill=(v,v,v+4,248))
    d.ellipse((38*q,35*q,154*q,151*q),outline=R(AMH,175),width=q); d.arc((34*q,31*q,158*q,155*q),195,315,fill=R(AMH,210),width=2*q); d.arc((34*q,31*q,158*q,155*q),20,150,fill=R(PUH,165),width=2*q)
    a=math.radians(angle); r1=37*q; r2=52*q; x1=c+math.cos(a)*r1; y1=c+math.sin(a)*r1; x2=c+math.cos(a)*r2; y2=c+math.sin(a)*r2
    d.line((x1,y1,x2,y2),fill=R(AMH),width=4*q); d.ellipse((x2-2*q,y2-2*q,x2+2*q,y2+2*q),fill=R(AMH))
    return ss(im,(cell,cell))

def knob_sheet():
    cell=192; im=Image.new('RGBA',(cell*8,cell*8),(0,0,0,0))
    for i in range(64): im.alpha_composite(knob(225+270*i/63,cell),((i%8)*cell,(i//8)*cell))
    return im

def button(size,state='off',radius=None):
    w,h=size; q=4; W,H=w*q,h*q; im=Image.new('RGBA',(W,H),(0,0,0,0)); d=ImageDraw.Draw(im); m=max(4,int(min(w,h)*.08))*q; r=(radius or min(w,h)*.22)*q; box=(m,m,W-m-1,H-m-1)
    sh=Image.new('RGBA',(W,H),(0,0,0,0)); sd=ImageDraw.Draw(sh); sd.rounded_rectangle((box[0],box[1]+6*q,box[2],box[3]+6*q),radius=r,fill=(0,0,0,165)); im.alpha_composite(sh.filter(ImageFilter.GaussianBlur(7*q)))
    d=ImageDraw.Draw(im); d.rounded_rectangle(box,radius=r,fill=(17,17,20,255) if state=='pressed' else (24,24,28,255),outline=(90,78,67,220),width=2*q)
    inn=(box[0]+7*q,box[1]+7*q,box[2]-7*q,box[3]-7*q); ir=max(3,r-6*q); d.rounded_rectangle(inn,radius=ir,fill=(33,33,38,255),outline=(10,10,12,255),width=2*q)
    d.arc(box,185,310,fill=R(AMH,220 if state=='on' else 105),width=2*q); d.arc(box,15,145,fill=R(PUH,165 if state=='on' else 80),width=2*q)
    if state=='on':
        g=Image.new('RGBA',(W,H),(0,0,0,0)); gd=ImageDraw.Draw(g); gd.rounded_rectangle(inn,radius=ir,outline=R(AM),width=3*q); im.alpha_composite(g.filter(ImageFilter.GaussianBlur(5*q))); d=ImageDraw.Draw(im); d.rounded_rectangle(inn,radius=ir,outline=R(AMH),width=2*q)
    return ss(im,size)

def power(state):
    im=button((160,160),state,76); d=ImageDraw.Draw(im); cx=80; cy=83; col=R(AMH) if state=='on' else (80,80,86,255)
    d.arc((49,52,111,114),-45,225,fill=col,width=5); d.line((80,43,80,77),fill=col,width=5); return im

def toggle(state):
    w,h=240,104; q=4; W,H=w*q,h*q; im=Image.new('RGBA',(W,H),(0,0,0,0)); d=ImageDraw.Draw(im); m=10*q; box=(m,m,W-m,H-m); r=(h/2-10)*q
    d.rounded_rectangle((m,m+5*q,W-m,H-m+5*q),radius=r,fill=(0,0,0,150)); d.rounded_rectangle(box,radius=r,fill=(18,18,21,255),outline=(78,66,60,255),width=2*q)
    inn=(m+6*q,m+6*q,W-m-6*q,H-m-6*q); d.rounded_rectangle(inn,radius=r-6*q,fill=(80,47,22,255) if state=='on' else (30,30,34,255),outline=R(PU,75),width=q)
    cx=(.70 if state=='on' else .30)*W; cy=H/2; rr=.34*h*q; d.ellipse((cx-rr,cy-rr,cx+rr,cy+rr),fill=(35,35,40,255),outline=R(AMH,200 if state=='on' else 110),width=2*q); d.arc((cx-rr,cy-rr,cx+rr,cy+rr),15,145,fill=R(PUH,150),width=2*q)
    return ss(im,(w,h))

def led(size,shape,color,on):
    w,h=size; q=4; W,H=w*q,h*q; im=Image.new('RGBA',(W,H),(0,0,0,0)); d=ImageDraw.Draw(im); m=8*q
    if shape=='line': box=(m,int(H*.35),W-m,int(H*.65)); rad=(box[3]-box[1])//2; inset=2*q
    else: box=(m,m,W-m,H-m); rad=(W-2*m)//2 if shape=='round' else 18*q; inset=7*q
    if shape=='round': d.ellipse(box,fill=(18,18,21,255),outline=(88,76,66,255),width=2*q)
    else: d.rounded_rectangle(box,radius=rad,fill=(18,18,21,255),outline=(88,76,66,255),width=2*q)
    inn=(box[0]+inset,box[1]+inset,box[2]-inset,box[3]-inset); ir=max(2,rad-inset); fill=R(color,230) if on else (23,23,28,255); outline=R(mix(color,(255,255,255),.48) if on else color,255 if on else 90)
    if on:
        g=Image.new('RGBA',(W,H),(0,0,0,0)); gd=ImageDraw.Draw(g)
        if shape=='round': gd.ellipse(inn,fill=R(color,220))
        else: gd.rounded_rectangle(inn,radius=ir,fill=R(color,220))
        im.alpha_composite(g.filter(ImageFilter.GaussianBlur(8*q))); d=ImageDraw.Draw(im)
    if shape=='round': d.ellipse(inn,fill=fill,outline=outline,width=q)
    else: d.rounded_rectangle(inn,radius=ir,fill=fill,outline=outline,width=q)
    return ss(im,size)

def slider_track(size,vertical=False):
    w,h=size; q=4; W,H=w*q,h*q; im=Image.new('RGBA',(W,H),(0,0,0,0)); d=ImageDraw.Draw(im)
    box=(int(W*.32),8*q,int(W*.68),H-8*q) if vertical else (8*q,int(H*.32),W-8*q,int(H*.68)); rad=(box[2]-box[0])//2 if vertical else (box[3]-box[1])//2
    d.rounded_rectangle(box,radius=rad,fill=(15,15,18,255),outline=(82,70,61,230),width=2*q); inn=(box[0]+4*q,box[1]+4*q,box[2]-4*q,box[3]-4*q); d.rounded_rectangle(inn,radius=max(2,rad-4*q),fill=(7,7,9,255),outline=R(PU,65),width=q); return ss(im,size)

def slider_fill(size,vertical,color):
    w,h=size; q=4; W,H=w*q,h*q; im=Image.new('RGBA',(W,H),(0,0,0,0)); d=ImageDraw.Draw(im)
    box=(int(W*.43),12*q,int(W*.57),H-12*q) if vertical else (12*q,int(H*.43),W-12*q,int(H*.57)); rad=(box[2]-box[0])//2 if vertical else (box[3]-box[1])//2
    d.rounded_rectangle(box,radius=max(2,rad),fill=R(color),outline=R(mix(color,(255,255,255),.45)),width=q); return ss(im,size)

def slider_thumb(shape):
    if shape=='rect': return button((104,128),'off',18)
    w=h=112; q=4; W=H=112*q; im=Image.new('RGBA',(W,H),(0,0,0,0)); d=ImageDraw.Draw(im); c=W/2; r=42*q
    d.ellipse((c-r,c-r+4*q,c+r,c+r+4*q),fill=(0,0,0,125)); d.ellipse((c-r,c-r,c+r,c+r),fill=(31,31,36,255),outline=R(CH,130),width=2*q); d.arc((c-r,c-r,c+r,c+r),190,315,fill=R(AMH,220),width=2*q); d.arc((c-r,c-r,c+r,c+r),10,145,fill=R(PUH,165),width=2*q); return ss(im,(112,112))

def screw(kind):
    w=h=128; q=4; W=H=w*q; im=Image.new('RGBA',(W,H),(0,0,0,0)); d=ImageDraw.Draw(im); c=W/2; r=50*q
    d.ellipse((c-r,c-r+5*q,c+r,c+r+5*q),fill=(0,0,0,140))
    for k in range(72):
        a0=2*math.pi*k/72; a1=2*math.pi*(k+1)/72; v=int(38+28*(math.cos(a0+2.15)+1)/2+6*math.sin(a0*3)); rr=45*q; d.polygon([(c,c),(c+math.cos(a0)*rr,c+math.sin(a0)*rr),(c+math.cos(a1)*rr,c+math.sin(a1)*rr)],fill=(v,v,v+3,248))
    d.ellipse((c-r,c-r,c+r,c+r),outline=R(CH,130),width=2*q); d.arc((c-r,c-r,c+r,c+r),190,315,fill=R(AMH,185),width=2*q); d.arc((c-r,c-r,c+r,c+r),10,145,fill=R(PUH,130),width=2*q); dark=(8,8,10,255)
    if kind=='phillips': d.rounded_rectangle((c-4*q,c-23*q,c+4*q,c+23*q),radius=2*q,fill=dark); d.rounded_rectangle((c-23*q,c-4*q,c+23*q,c+4*q),radius=2*q,fill=dark)
    elif kind=='slot': d.rounded_rectangle((c-29*q,c-4*q,c+29*q,c+4*q),radius=2*q,fill=dark)
    elif kind=='hex': d.polygon([(c+math.cos(math.radians(30+i*60))*22*q,c+math.sin(math.radians(30+i*60))*22*q) for i in range(6)],fill=dark)
    elif kind=='torx': d.polygon([(c+math.cos(math.radians(-90+i*30))*(23 if i%2==0 else 11)*q,c+math.sin(math.radians(-90+i*30))*(23 if i%2==0 else 11)*q) for i in range(12)],fill=dark)
    elif kind=='knurled':
        for i in range(28):
            a=2*math.pi*i/28; d.line((c+math.cos(a)*37*q,c+math.sin(a)*37*q,c+math.cos(a)*44*q,c+math.sin(a)*44*q),fill=(110,88,67,190),width=2*q)
    elif kind=='washer':
        a=im.getchannel('A'); ImageDraw.Draw(a).ellipse((c-20*q,c-20*q,c+20*q,c+20*q),fill=0); im.putalpha(a)
    return ss(im,(w,h))

def main():
    for d in [K,B,BT,L,S,SC]:
        for p in d.glob('*.png'): p.unlink()
    paths=[]; p=B/'panel_bg.png'; panel().save(p,optimize=True,compress_level=9); paths.append(p); p=K/'knob_sprite_64.png'; knob_sheet().save(p,optimize=True,compress_level=9); paths.append(p)
    for n,z in [('square_small',(96,96)),('square_large',(160,160)),('rect_small',(220,84)),('rect_large',(340,120)),('capsule',(320,104))]:
        for st in ['off','on','pressed']:
            p=BT/f'button_{n}_{st}.png'; button(z,st,(min(z)//2-8) if n=='capsule' else None).save(p,optimize=True); paths.append(p)
    for st in ['off','on','pressed']:
        p=BT/f'button_power_{st}.png'; power(st).save(p,optimize=True); paths.append(p)
    for st in ['off','on']:
        p=BT/f'button_toggle_{st}.png'; toggle(st).save(p,optimize=True); paths.append(p)
    for sh,z in [('round',(96,96)),('square',(96,96)),('tiny',(48,48)),('line',(180,44))]:
        for cn,col in [('amber',AM),('purple',PU)]:
            for st in ['off','on']:
                p=L/f'led_{sh}_{cn}_{st}.png'; led(z,'round' if sh=='tiny' else sh,col,st=='on').save(p,optimize=True); paths.append(p)
    for o,z in [('h',(512,72)),('v',(72,512))]:
        v=o=='v'; p=S/f'slider_{o}_track.png'; slider_track(z,v).save(p,optimize=True); paths.append(p)
        for cn,col in [('amber',AM),('purple',PU)]:
            p=S/f'slider_{o}_fill_{cn}.png'; slider_fill(z,v,col).save(p,optimize=True); paths.append(p)
    for sh in ['round','rect']:
        p=S/f'slider_thumb_{sh}.png'; slider_thumb(sh).save(p,optimize=True); paths.append(p)
    for k in ['phillips','slot','hex','torx','knurled','washer']:
        p=SC/f'screw_{k}.png'; screw(k).save(p,optimize=True); paths.append(p)
    entries=[]
    for p in sorted(paths):
        im=Image.open(p); entries.append({'path':p.relative_to(OUT).as_posix(),'width':im.width,'height':im.height,'mode':im.mode,'alpha':'A' in im.getbands()})
    (OUT/'asset_manifest.json').write_text(json.dumps({'theme':'Obsidian Luxury × Industrial Pro × Deep Purple','background':'1500x930','knob_matrix':'8x8 / 64','assets':entries},indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    lines=['# VVChain UI PNG Assets','','Theme: **Obsidian Luxury × Industrial Pro × Deep Purple**.','', '- `backgrounds/panel_bg.png`: exact 1500×930 brushed metal, no text/UI placeholders.', '- `knobs/knob_sprite_64.png`: 8×8 / 64 frames, transparent RGBA.', '- `components/*`: isolated transparent PNG components; no labels.','', '## Files','']+[f"- `{e['path']}` — {e['width']}×{e['height']} {e['mode']} alpha={str(e['alpha']).lower()}" for e in entries]
    (OUT/'ASSET_INDEX.md').write_text('\n'.join(lines)+'\n',encoding='utf-8'); print(f'Generated {len(paths)} PNG assets')

if __name__=='__main__': main()
