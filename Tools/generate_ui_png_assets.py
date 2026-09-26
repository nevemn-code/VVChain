#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
from math import atan2, cos, pi, sin, sqrt
import hashlib
import json
import random

from PIL import Image, ImageDraw, ImageFilter, ImageOps

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "assets" / "ui" / "png"
OUT.mkdir(parents=True, exist_ok=True)

SEED = 10710071
rng = random.Random(SEED)

PANEL_SIZE = (1500, 930)
MODULE_SIZE = (600, 930)
GRAPH_SIZE = (1500, 330)
KNOB_FRAME = 128
KNOB_GRID = 8
BUTTON_SIZE = (360, 120)
LED_SIZE = 128
SCREW_SIZE = 96
SLIDER_TRACK_SIZE = (800, 120)
SLIDER_THUMB_SIZE = 120
POWER_SIZE = 180

THEMES = {
    "ivory": {
        "panel_hi": (247, 241, 228), "panel_mid": (226, 214, 194), "panel_lo": (191, 174, 149),
        "frame": (118, 73, 18), "frame_hi": (255, 192, 70), "frame_lo": (51, 29, 8),
        "accent": (255, 168, 19), "accent_hi": (255, 232, 149),
        "glass_hi": (40, 36, 27), "glass_lo": (9, 10, 8),
        "metal_hi": (255, 254, 249), "metal_mid": (178, 174, 166), "metal_lo": (49, 48, 45),
        "rubber_hi": (49, 47, 42), "rubber_lo": (13, 13, 12), "text": (38, 31, 24),
    },
    "studio": {
        "panel_hi": (73, 80, 82), "panel_mid": (42, 49, 51), "panel_lo": (18, 25, 27),
        "frame": (111, 58, 37), "frame_hi": (214, 125, 72), "frame_lo": (31, 17, 12),
        "accent": (72, 229, 222), "accent_hi": (184, 255, 250),
        "glass_hi": (15, 50, 54), "glass_lo": (4, 13, 15),
        "metal_hi": (244, 245, 241), "metal_mid": (145, 151, 151), "metal_lo": (48, 55, 57),
        "rubber_hi": (47, 54, 56), "rubber_lo": (8, 13, 15), "text": (233, 241, 241),
    },
}

def clamp(v, lo=0, hi=255): return max(lo, min(hi, int(round(v))))
def mix(a, b, t): return tuple(clamp(x + (y-x)*t) for x, y in zip(a,b))
def rgba(c, a=255): return tuple(c) + (a,)

def save_rgba(im: Image.Image, path: Path):
    im.convert("RGBA").save(path, "PNG", optimize=True)

def shadow_canvas(size, ellipse_box=None, rect_box=None, radius=12, blur=10, alpha=130, offset=(0,5)):
    sh = Image.new("RGBA", size, (0,0,0,0))
    d = ImageDraw.Draw(sh, "RGBA")
    ox, oy = offset
    if ellipse_box:
        x0,y0,x1,y1 = ellipse_box
        d.ellipse((x0+ox,y0+oy,x1+ox,y1+oy), fill=(0,0,0,alpha))
    if rect_box:
        x0,y0,x1,y1 = rect_box
        d.rounded_rectangle((x0+ox,y0+oy,x1+ox,y1+oy), radius=radius, fill=(0,0,0,alpha))
    return sh.filter(ImageFilter.GaussianBlur(blur))

def brushed_surface(theme, size, module=False, disabled=False):
    w,h=size
    im=Image.new("RGBA",size,(0,0,0,255))
    px=im.load()
    hi,lo=theme["panel_hi"],theme["panel_lo"]
    for y in range(h):
        fy=y/max(1,h-1)
        base=mix(hi,lo,0.30+0.52*fy)
        horizontal=3.4*sin(y*.61)+1.8*sin(y*2.37)+0.8*sin(y*5.41)
        for x in range(w):
            fx=x/max(1,w-1)
            broad=15*cos((fx-.50)*pi)+7*cos((fx-.16)*pi*3)
            grain=horizontal+1.4*sin(x*.083+y*.017)+rng.uniform(-2.2,2.2)
            edge=-20*(abs(fx-.5)**1.7)-9*(abs(fy-.5)**1.9)
            col=tuple(clamp(c+broad+grain+edge) for c in base)
            px[x,y]=col+(255,)
    if disabled:
        gray=ImageOps.grayscale(im.convert("RGB"))
        im=Image.merge("RGBA",(gray,gray,gray,Image.new("L",size,255)))
        im=Image.alpha_composite(im,Image.new("RGBA",size,(38,41,42,42)))
    return im

def graph_frame(theme, disabled=False):
    w,h=GRAPH_SIZE
    im=Image.new("RGBA",(w,h),(0,0,0,0))
    im.alpha_composite(shadow_canvas((w,h),rect_box=(20,16,w-21,h-24),radius=24,blur=14,alpha=150,offset=(0,7)))
    d=ImageDraw.Draw(im,"RGBA")
    frame=theme["frame"] if not disabled else (95,95,95)
    hi=theme["frame_hi"] if not disabled else (176,176,176)
    d.rounded_rectangle((7,7,w-8,h-15),radius=26,fill=(18,17,14,255),outline=frame,width=5)
    d.rounded_rectangle((13,13,w-14,h-21),radius=21,outline=hi+(210,),width=2)
    inner=(31,30,w-32,h-39)
    glass=Image.new("RGBA",(w,h),(0,0,0,0)); gp=glass.load()
    g0=theme["glass_hi"] if not disabled else (45,45,45)
    g1=theme["glass_lo"] if not disabled else (12,12,12)
    for y in range(inner[1],inner[3]):
        t=(y-inner[1])/max(1,inner[3]-inner[1]-1); col=mix(g0,g1,t)
        for x in range(inner[0],inner[2]): gp[x,y]=col+(255,)
    mask=Image.new("L",(w,h),0); md=ImageDraw.Draw(mask)
    md.rounded_rectangle(inner,radius=17,fill=255)
    im.alpha_composite(Image.composite(glass,Image.new("RGBA",(w,h),(0,0,0,0)),mask))
    refl=Image.new("RGBA",(w,h),(0,0,0,0)); rd=ImageDraw.Draw(refl,"RGBA")
    rd.polygon([(50,45),(660,45),(470,225),(50,225)],fill=(255,255,255,13 if not disabled else 7))
    rd.ellipse((70,-80,500,120),fill=(255,220,140,18 if not disabled else 6))
    im.alpha_composite(refl.filter(ImageFilter.GaussianBlur(18)))
    return im

def knob_frame(theme, angle_deg, disabled=False):
    s=KNOB_FRAME; c=s/2
    im=Image.new("RGBA",(s,s),(0,0,0,0))
    im.alpha_composite(shadow_canvas((s,s),ellipse_box=(15,18,s-15,s-10),blur=8,alpha=145,offset=(0,5)))
    d=ImageDraw.Draw(im,"RGBA")
    R=46.0
    tick_col=(52,43,31) if not disabled and theme is THEMES["ivory"] else ((221,232,232) if not disabled else (90,90,90))
    for i in range(21):
        a=(-135+i*270/20)*pi/180
        r0=R+4; r1=R+(10 if i%5==0 else 8)
        d.line((c+cos(a)*r0,c+sin(a)*r0,c+cos(a)*r1,c+sin(a)*r1),
               fill=tick_col+(220 if i%5==0 else 125,),width=2 if i%5==0 else 1)
    frame=(111,111,111) if disabled else theme["frame"]
    accent=(145,145,145) if disabled else theme["accent"]
    d.ellipse((c-R-3,c-R-3,c+R+3,c+R+3),
              fill=rgba(theme["rubber_lo"] if not disabled else (31,31,31)),outline=frame,width=3)
    for i in range(72):
        a=i*2*pi/72; r0=R-.5; r1=R+2.5
        col=(75,75,75,135) if disabled else (8,8,8,145)
        d.line((c+cos(a)*r0,c+sin(a)*r0,c+cos(a)*r1,c+sin(a)*r1),fill=col,width=1)
    d.ellipse((c-R+2,c-R+2,c+R-2,c+R-2),outline=accent+(245,),width=3)
    d.ellipse((c-R+6,c-R+6,c+R-6,c+R-6),fill=(24,24,23,255),outline=(255,255,255,45),width=1)
    face=Image.new("RGBA",(s,s),(0,0,0,0)); fp=face.load(); r=R-10
    hi,mid,lo=theme["metal_hi"],theme["metal_mid"],theme["metal_lo"]
    if disabled: hi,mid,lo=(224,224,220),(135,135,133),(55,55,55)
    for y in range(s):
        dy=y-c
        for x in range(s):
            dx=x-c; rr=sqrt(dx*dx+dy*dy)
            if rr>r: continue
            th=atan2(dy,dx)
            cone=.5+.5*sin(th*4.0+.8); cone2=.5+.5*sin(th*2.0-1.3)
            radial=max(0,min(1,1-rr/r)); spec=max(0,cos(th+2.35))**9
            v=.17+.34*cone+.17*cone2+.16*radial+.30*spec
            col=mix(lo,mid,min(1,v/.48)) if v<.48 else mix(mid,hi,min(1,(v-.48)/.52))
            grain=rng.uniform(-2.0,2.0)+1.1*sin(rr*2.2)
            fp[x,y]=tuple(clamp(q+grain) for q in col)+(255,)
    im.alpha_composite(face.filter(ImageFilter.GaussianBlur(.20)))
    d=ImageDraw.Draw(im,"RGBA")
    d.ellipse((c-r,c-r,c+r,c+r),outline=(255,255,255,68),width=1)
    sp=Image.new("RGBA",(s,s),(0,0,0,0)); sd=ImageDraw.Draw(sp,"RGBA")
    sd.arc((c-r*.84,c-r*.84,c+r*.84,c+r*.84),205,320,
           fill=(255,255,255,132 if not disabled else 65),width=4)
    im.alpha_composite(sp.filter(ImageFilter.GaussianBlur(1.1)))
    a=angle_deg*pi/180; plen=r*.68; ex=c+cos(a)*plen; ey=c+sin(a)*plen
    d=ImageDraw.Draw(im,"RGBA")
    d.line((c,c,ex,ey),fill=(4,4,4,235),width=6)
    d.line((c,c,ex,ey),fill=(34,32,28,255) if not disabled else (65,65,65,255),width=3)
    d.ellipse((c-2.5,c-2.5,c+2.5,c+2.5),fill=(32,32,32,255))
    return im

def knob_strip(theme, disabled=False):
    out=Image.new("RGBA",(KNOB_FRAME*8,KNOB_FRAME*8),(0,0,0,0))
    for i in range(64):
        a=-125+250*i/63
        out.alpha_composite(knob_frame(theme,a,disabled),((i%8)*KNOB_FRAME,(i//8)*KNOB_FRAME))
    return out

def button(theme,state,disabled=False):
    w,h=BUTTON_SIZE
    im=Image.new("RGBA",(w,h),(0,0,0,0))
    im.alpha_composite(shadow_canvas((w,h),rect_box=(10,9,w-11,h-18),radius=13,blur=9,
                                     alpha=145 if not disabled else 90,offset=(0,6)))
    body=Image.new("RGBA",(w,h),(0,0,0,0)); bp=body.load()
    top=(46,39,29) if theme is THEMES["ivory"] else (32,42,44)
    bot=(8,7,5) if theme is THEMES["ivory"] else (6,11,12)
    if disabled: top,bot=(58,58,57),(20,20,20)
    if state=="pressed": top=mix(top,(133,76,30),.18)
    for y in range(10,h-22):
        t=(y-10)/max(1,h-33); col=mix(top,bot,t)
        for x in range(12,w-12): bp[x,y]=col+(255,)
    mask=Image.new("L",(w,h),0); md=ImageDraw.Draw(mask)
    md.rounded_rectangle((9,7,w-10,h-20),radius=14,fill=255)
    im.alpha_composite(Image.composite(body,Image.new("RGBA",(w,h),(0,0,0,0)),mask))
    d=ImageDraw.Draw(im,"RGBA")
    edge=(140,140,140) if disabled else (theme["accent"] if state=="on" else theme["frame_hi"])
    d.rounded_rectangle((7,5,w-8,h-18),radius=15,outline=edge,width=4)
    d.rounded_rectangle((12,10,w-13,h-23),radius=10,outline=(255,255,255,42 if not disabled else 20),width=2)
    if state=="on" and not disabled:
        glow=Image.new("RGBA",(w,h),(0,0,0,0)); gd=ImageDraw.Draw(glow,"RGBA")
        gd.rounded_rectangle((6,4,w-7,h-17),radius=16,outline=theme["accent"]+(105,),width=9)
        im.alpha_composite(glow.filter(ImageFilter.GaussianBlur(7)))
    return im

def led(theme,state):
    s=LED_SIZE; im=Image.new("RGBA",(s,s),(0,0,0,0))
    red=state=="red"; disabled=state=="disabled"; on=state in ("amber","red")
    glow=(237,48,40) if red else (theme["accent"] if not disabled else (125,125,125))
    if on:
        gl=Image.new("RGBA",(s,s),(0,0,0,0)); gd=ImageDraw.Draw(gl,"RGBA")
        gd.ellipse((19,19,s-19,s-19),fill=glow+(105,))
        im.alpha_composite(gl.filter(ImageFilter.GaussianBlur(13)))
    d=ImageDraw.Draw(im,"RGBA")
    d.ellipse((28,28,s-28,s-28),fill=(20,20,19,255),
              outline=((100,100,100) if disabled else theme["frame"]),width=4)
    core=(83,83,83) if state in ("off","disabled") else glow
    d.ellipse((38,38,s-38,s-38),fill=core+(255,),outline=(255,255,255,72 if on else 36),width=2)
    d.ellipse((45,42,58,50),fill=(255,255,255,170 if on else 55))
    return im

def screw(theme,disabled=False):
    s=SCREW_SIZE
    im=Image.new("RGBA",(s,s),(0,0,0,0))
    im.alpha_composite(shadow_canvas((s,s),ellipse_box=(19,17,s-19,s-24),blur=5,alpha=100,offset=(0,5)))
    d=ImageDraw.Draw(im,"RGBA"); frame=(105,105,105) if disabled else theme["frame"]
    d.ellipse((18,13,s-19,s-24),fill=(161,157,150,255) if not disabled else (145,145,145,255),outline=frame,width=3)
    d.ellipse((24,19,s-25,s-30),fill=(228,222,212,255) if not disabled else (194,194,194,255))
    d.line((34,53,61,42),fill=(52,48,43,255),width=6)
    return im

def slider_track(theme,disabled=False):
    w,h=SLIDER_TRACK_SIZE
    im=Image.new("RGBA",(w,h),(0,0,0,0))
    im.alpha_composite(shadow_canvas((w,h),rect_box=(16,28,w-17,h-34),radius=12,blur=8,alpha=135,offset=(0,5)))
    d=ImageDraw.Draw(im,"RGBA"); frame=(110,110,110) if disabled else theme["frame_hi"]
    d.rounded_rectangle((14,23,w-15,h-31),radius=13,fill=(12,12,12,255),outline=frame,width=3)
    d.rounded_rectangle((28,42,w-29,h-50),radius=6,
                        fill=(38,37,34,255) if not disabled else (55,55,55,255),
                        outline=(255,255,255,35),width=1)
    return im

def slider_thumb(theme,disabled=False):
    s=SLIDER_THUMB_SIZE
    im=Image.new("RGBA",(s,s),(0,0,0,0))
    im.alpha_composite(shadow_canvas((s,s),rect_box=(22,17,s-23,s-28),radius=13,blur=8,alpha=135,offset=(0,6)))
    d=ImageDraw.Draw(im,"RGBA"); accent=(125,125,125) if disabled else theme["accent"]
    d.rounded_rectangle((20,14,s-21,s-31),radius=13,
                        fill=(50,49,46,255) if not disabled else (70,70,70,255),outline=accent,width=3)
    d.rounded_rectangle((28,22,s-29,s-39),radius=8,
                        fill=(180,175,165,255) if not disabled else (145,145,145,255),
                        outline=(255,255,255,60),width=2)
    return im

def power(theme,on,disabled=False):
    im=knob_frame(theme,-90,disabled).resize((POWER_SIZE,POWER_SIZE),Image.Resampling.LANCZOS)
    d=ImageDraw.Draw(im,"RGBA"); c=POWER_SIZE/2
    col=(155,155,155) if disabled else (theme["accent"] if on else (82,84,82))
    d.line((c,c-31,c,c-7),fill=rgba(col),width=8)
    d.arc((c-30,c-26,c+30,c+34),-50,230,fill=rgba(col),width=8)
    return im

def generate_theme(name):
    t=THEMES[name]
    save_rgba(brushed_surface(t,PANEL_SIZE),OUT/f"{name}_panel.png")
    save_rgba(brushed_surface(t,MODULE_SIZE,module=True),OUT/f"{name}_module.png")
    save_rgba(graph_frame(t),OUT/f"{name}_graph.png")
    save_rgba(knob_strip(t),OUT/f"{name}_knob_strip.png")
    for st in ("off","on","pressed"):
        save_rgba(button(t,st),OUT/f"{name}_button_{st}.png")
    save_rgba(led(t,"off"),OUT/f"{name}_led_off.png")
    save_rgba(led(t,"amber"),OUT/f"{name}_led_on.png")
    save_rgba(power(t,False),OUT/f"{name}_power_off.png")
    save_rgba(power(t,True),OUT/f"{name}_power_on.png")
    save_rgba(screw(t),OUT/f"{name}_screw.png")
    save_rgba(slider_track(t),OUT/f"{name}_slider_track.png")
    save_rgba(slider_thumb(t),OUT/f"{name}_slider_thumb.png")

def generate_muted():
    base=THEMES["ivory"]
    save_rgba(brushed_surface(base,PANEL_SIZE,disabled=True),OUT/"muted_panel.png")
    save_rgba(brushed_surface(base,MODULE_SIZE,module=True,disabled=True),OUT/"muted_module.png")
    save_rgba(graph_frame(base,disabled=True),OUT/"muted_graph.png")
    save_rgba(knob_strip(base,disabled=True),OUT/"muted_knob_strip.png")
    for st in ("off","on","pressed"):
        save_rgba(button(base,st,disabled=True),OUT/f"muted_button_{st}.png")
    save_rgba(led(base,"disabled"),OUT/"muted_led_off.png")
    save_rgba(led(base,"disabled"),OUT/"muted_led_on.png")
    save_rgba(power(base,False,disabled=True),OUT/"muted_power_off.png")
    save_rgba(power(base,True,disabled=True),OUT/"muted_power_on.png")
    save_rgba(screw(base,disabled=True),OUT/"muted_screw.png")
    save_rgba(slider_track(base,disabled=True),OUT/"muted_slider_track.png")
    save_rgba(slider_thumb(base,disabled=True),OUT/"muted_slider_thumb.png")
    save_rgba(button(base,"off",disabled=True),OUT/"button_disabled.png")
    save_rgba(led(base,"red"),OUT/"bypass_led_red.png")

def main():
    # Keep the production asset directory deterministic. Remove obsolete
    # runtime PNGs from earlier generators before rendering the v1.0.71 set.
    for stale in OUT.glob("*.png"):
        stale.unlink()
    for stale_name in ("MANIFEST.txt",):
        stale_path = OUT / stale_name
        if stale_path.exists():
            stale_path.unlink()

    generate_theme("ivory")
    generate_theme("studio")
    generate_muted()
    manifest={}
    for p in sorted(OUT.glob("*.png")):
        im=Image.open(p)
        b=p.read_bytes()
        manifest[p.name]={
            "width":im.width,"height":im.height,"mode":im.mode,
            "sha256":hashlib.sha256(b).hexdigest(),"bytes":len(b)
        }
    for n in ("ivory_knob_strip.png","studio_knob_strip.png","muted_knob_strip.png"):
        manifest[n].update({"grid":"8x8","frameWidth":128,"frameHeight":128,"frames":64})
    (OUT/"asset_manifest.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
    print(f"Generated {len(manifest)} engineering RGBA PNG assets in {OUT}")

if __name__ == "__main__":
    main()
