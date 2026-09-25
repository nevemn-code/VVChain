#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
from math import atan2, cos, pi, sin, sqrt
import random

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "assets" / "ui" / "png"
OUT.mkdir(parents=True, exist_ok=True)

SEED = 670067
rng = random.Random(SEED)

THEMES = {
    "ivory": {
        "panel_top": (244, 238, 225),
        "panel_bottom": (211, 199, 180),
        "frame": (151, 102, 30),
        "frame_hi": (239, 184, 72),
        "text": (53, 42, 32),
        "accent": (255, 171, 25),
        "accent_hi": (255, 223, 129),
        "glass": (18, 18, 15),
        "metal_hi": (248, 247, 243),
        "metal_mid": (164, 162, 155),
        "metal_lo": (63, 64, 62),
    },
    "studio": {
        "panel_top": (67, 73, 76),
        "panel_bottom": (17, 24, 27),
        "frame": (145, 81, 55),
        "frame_hi": (206, 121, 77),
        "text": (232, 241, 241),
        "accent": (79, 228, 223),
        "accent_hi": (184, 255, 251),
        "glass": (5, 31, 36),
        "metal_hi": (238, 239, 235),
        "metal_mid": (146, 152, 152),
        "metal_lo": (55, 62, 64),
    },
    "muted": {
        "panel_top": (196, 196, 193),
        "panel_bottom": (105, 108, 107),
        "frame": (89, 89, 89),
        "frame_hi": (158, 158, 158),
        "text": (232, 232, 232),
        "accent": (164, 164, 164),
        "accent_hi": (220, 220, 220),
        "glass": (20, 22, 23),
        "metal_hi": (224, 224, 221),
        "metal_mid": (139, 139, 136),
        "metal_lo": (57, 59, 59),
    },
}


def lerp(a, b, t):
    return int(round(a + (b - a) * t))


def mix(c1, c2, t):
    return tuple(lerp(a, b, t) for a, b in zip(c1, c2))


def rounded_mask(size, radius):
    m = Image.new("L", size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, size[0]-1, size[1]-1), radius=radius, fill=255)
    return m


def add_shadow(base, box, radius, alpha=110, blur=8, offset=(0, 4)):
    layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    x0, y0, x1, y1 = box
    ox, oy = offset
    d.rounded_rectangle((x0+ox, y0+oy, x1+ox, y1+oy), radius=radius, fill=(0, 0, 0, alpha))
    layer = layer.filter(ImageFilter.GaussianBlur(blur))
    base.alpha_composite(layer)


def brushed_panel(theme, size=(512, 512), bypass=False):
    w, h = size
    t = THEMES["muted"] if bypass else theme
    im = Image.new("RGBA", size, (0, 0, 0, 255))
    px = im.load()
    top, bottom = t["panel_top"], t["panel_bottom"]
    for y in range(h):
        fy = y / max(1, h-1)
        base = mix(top, bottom, fy)
        line = 4.5*sin(y*0.34) + 2.0*sin(y*1.73)
        for x in range(w):
            grain = line + 2.2*sin((x*0.037)+(y*0.012)) + rng.uniform(-2.5, 2.5)
            vign = -10.0 * abs((x/(w-1))-0.5)
            px[x, y] = tuple(max(0, min(255, int(c + grain + vign))) for c in base) + (255,)
    d = ImageDraw.Draw(im, "RGBA")
    d.rounded_rectangle((2, 2, w-3, h-3), radius=18, outline=t["frame"], width=4)
    d.rounded_rectangle((8, 8, w-9, h-9), radius=14, outline=(255,255,255,45), width=2)
    d.line((16, 13, w-16, 13), fill=(255,255,255,45), width=2)
    return im


def graph_bezel(theme, size=(1024, 256)):
    w, h = size
    im = Image.new("RGBA", size, (0, 0, 0, 0))
    add_shadow(im, (9, 9, w-10, h-10), 20, alpha=150, blur=13, offset=(0,5))
    d = ImageDraw.Draw(im, "RGBA")
    d.rounded_rectangle((5,5,w-6,h-6), radius=22, fill=(8,9,9,255), outline=theme["frame"], width=4)
    d.rounded_rectangle((12,12,w-13,h-13), radius=17, fill=theme["glass"]+(255,), outline=(255,255,255,28), width=2)
    # subtle glass gradient + diagonal reflection
    overlay = Image.new("RGBA", size, (0,0,0,0))
    od = ImageDraw.Draw(overlay, "RGBA")
    od.polygon([(30,28),(w*0.58,28),(w*0.37,h-28),(30,h-28)], fill=(255,255,255,13))
    od.line((20,20,w-20,20), fill=(255,255,255,25), width=2)
    overlay = overlay.filter(ImageFilter.GaussianBlur(3))
    im.alpha_composite(overlay)
    return im


def metal_disc(size, theme, angle_deg, muted=False):
    w = h = size
    c = (w-1)/2.0
    im = Image.new("RGBA", (w,h), (0,0,0,0))
    # shadow
    shadow = Image.new("RGBA", (w,h), (0,0,0,0))
    sd = ImageDraw.Draw(shadow)
    sd.ellipse((10,14,w-10,h-6), fill=(0,0,0,150))
    shadow = shadow.filter(ImageFilter.GaussianBlur(max(2,size//20)))
    im.alpha_composite(shadow)

    d = ImageDraw.Draw(im, "RGBA")
    R = size*0.34
    cx = cy = c
    accent = THEMES["muted"]["accent"] if muted else theme["accent"]
    frame = THEMES["muted"]["frame"] if muted else theme["frame"]

    # ticks
    for i in range(21):
        a = (-135 + i*(270/20))*pi/180.0
        r0 = R+7
        r1 = R+(15 if i%5==0 else 12)
        col = theme["text"] if not muted else (65,65,65)
        alpha = 220 if i%5==0 else 130
        width = 3 if i%5==0 else 2
        d.line((cx+cos(a)*r0, cy+sin(a)*r0, cx+cos(a)*r1, cy+sin(a)*r1), fill=col+(alpha,), width=width)

    d.ellipse((cx-R-6, cy-R-6, cx+R+6, cy+R+6), fill=(16,17,17,255), outline=frame, width=3)
    d.ellipse((cx-R-2, cy-R-2, cx+R+2, cy+R+2), outline=accent+(235,), width=3)

    # conical brushed metal pixel fill
    r = R-6
    disc = Image.new("RGBA", (w,h), (0,0,0,0))
    pp = disc.load()
    hi, mid, lo = theme["metal_hi"], theme["metal_mid"], theme["metal_lo"]
    if muted:
        hi, mid, lo = THEMES["muted"]["metal_hi"], THEMES["muted"]["metal_mid"], THEMES["muted"]["metal_lo"]
    for y in range(h):
        dy = y-cy
        for x in range(w):
            dx = x-cx
            rr = sqrt(dx*dx+dy*dy)
            if rr > r:
                continue
            th = atan2(dy,dx)
            cone = 0.5 + 0.5*sin(th*2.0 + 0.55)
            radial = max(0.0, min(1.0, 1.0-rr/r))
            shine = max(0.0, cos(th+2.25))**7
            v = 0.18 + 0.48*cone + 0.18*radial + 0.26*shine
            if v < 0.45:
                col = mix(lo, mid, v/0.45)
            else:
                col = mix(mid, hi, min(1.0,(v-0.45)/0.55))
            noise = rng.uniform(-3.0,3.0) + 1.4*sin(rr*1.7)
            pp[x,y] = tuple(max(0,min(255,int(c0+noise))) for c0 in col)+(255,)
    disc = disc.filter(ImageFilter.GaussianBlur(0.22))
    im.alpha_composite(disc)

    d = ImageDraw.Draw(im, "RGBA")
    d.ellipse((cx-r,cy-r,cx+r,cy+r), outline=(255,255,255,55), width=2)
    # specular crescent
    spec = Image.new("RGBA",(w,h),(0,0,0,0)); sdraw=ImageDraw.Draw(spec,"RGBA")
    sdraw.arc((cx-r*0.82,cy-r*0.82,cx+r*0.82,cy+r*0.82), 212, 315, fill=(255,255,255,100), width=4)
    spec=spec.filter(ImageFilter.GaussianBlur(1.2)); im.alpha_composite(spec)

    # pointer and amber tip
    a = angle_deg*pi/180.0
    plen = r*0.64
    ex, ey = cx+cos(a)*plen, cy+sin(a)*plen
    d = ImageDraw.Draw(im, "RGBA")
    d.line((cx,cy,ex,ey), fill=(10,10,10,235), width=max(3,size//18))
    d.line((cx,cy,ex,ey), fill=((40,40,40,255) if muted else (35,33,28,255)), width=max(2,size//28))
    tip = Image.new("RGBA",(w,h),(0,0,0,0)); td=ImageDraw.Draw(tip,"RGBA")
    glow_col = (160,160,160) if muted else accent
    td.ellipse((ex-7,ey-7,ex+7,ey+7), fill=glow_col+(210,))
    tip=tip.filter(ImageFilter.GaussianBlur(4)); im.alpha_composite(tip)
    d=ImageDraw.Draw(im,"RGBA")
    d.rounded_rectangle((ex-4,ey-4,ex+4,ey+4), radius=2, fill=(245,245,245,220) if muted else theme["accent_hi"]+(255,))
    d.ellipse((cx-3,cy-3,cx+3,cy+3), fill=(35,35,35,255))
    return im


def knob_strip(theme_name, muted=False):
    theme = THEMES[theme_name]
    frame = 96
    strip = Image.new("RGBA",(frame*8,frame*8),(0,0,0,0))
    for i in range(64):
        angle = -125 + (250.0*i/63.0)
        strip.alpha_composite(metal_disc(frame, theme, angle, muted=muted), ((i%8)*frame,(i//8)*frame))
    return strip


def button(theme, state, size=(300,78), muted=False):
    w,h=size
    t=THEMES["muted"] if muted else theme
    im=Image.new("RGBA",size,(0,0,0,0))
    add_shadow(im,(10,8,w-11,h-13),12,alpha=150 if state!="pressed" else 90,blur=7,offset=(0,5))
    d=ImageDraw.Draw(im,"RGBA")
    top=(34,31,26) if theme is THEMES["ivory"] else (28,35,37)
    bottom=(7,7,6)
    if muted: top,bottom=(55,57,57),(18,19,19)
    if state=="pressed": top=mix(top,(255,138,45),0.12); bottom=(13,8,4)
    body=Image.new("RGBA",size,(0,0,0,0))
    bp=body.load()
    for y in range(h):
        c=mix(top,bottom,y/max(1,h-1))
        for x in range(w):
            bp[x,y]=c+(255,)
    mask=rounded_mask(size,12)
    im.alpha_composite(Image.composite(body,Image.new("RGBA",size,(0,0,0,0)),mask))
    d=ImageDraw.Draw(im,"RGBA")
    outline=t["accent"] if state=="on" else t["frame_hi"]
    d.rounded_rectangle((5,4,w-6,h-13),radius=12,outline=outline,width=3)
    d.rounded_rectangle((10,9,w-11,h-18),radius=8,outline=(255,255,255,35),width=2)
    if state=="on":
        glow=Image.new("RGBA",size,(0,0,0,0)); gd=ImageDraw.Draw(glow,"RGBA")
        gd.rounded_rectangle((4,3,w-5,h-12),radius=13,outline=t["accent"]+(120,),width=7)
        glow=glow.filter(ImageFilter.GaussianBlur(6)); im.alpha_composite(glow)
    return im


def led(theme, on, red=False, muted=False, size=80):
    t=THEMES["muted"] if muted else theme
    im=Image.new("RGBA",(size,size),(0,0,0,0))
    c=size//2
    glow=(235,45,38) if red else t["accent"]
    if on:
        gl=Image.new("RGBA",im.size,(0,0,0,0)); gd=ImageDraw.Draw(gl,"RGBA")
        gd.ellipse((10,10,size-10,size-10),fill=glow+(115,))
        gl=gl.filter(ImageFilter.GaussianBlur(10)); im.alpha_composite(gl)
    d=ImageDraw.Draw(im,"RGBA")
    d.ellipse((14,14,size-14,size-14),fill=(18,18,18,255),outline=t["frame"],width=3)
    core=(95,95,95) if not on else glow
    d.ellipse((22,22,size-22,size-22),fill=core+(255,),outline=(255,255,255,60),width=2)
    d.ellipse((28,25,37,32),fill=(255,255,255,150 if on else 50))
    return im


def power(theme, on, muted=False):
    im=metal_disc(160,theme,-90,muted=muted)
    d=ImageDraw.Draw(im,"RGBA")
    col=(180,180,180) if muted else (theme["accent"] if on else (75,78,78))
    d.line((80,52,80,77),fill=col+(255,),width=7)
    d.arc((52,55,108,111),-48,228,fill=col+(255,),width=7)
    return im


def screw(theme, size=64):
    im=Image.new("RGBA",(size,size),(0,0,0,0))
    add_shadow(im,(13,10,size-14,size-17),20,alpha=100,blur=4,offset=(0,4))
    d=ImageDraw.Draw(im,"RGBA")
    d.ellipse((12,8,size-13,size-17),fill=(169,161,151,255),outline=theme["frame"],width=2)
    d.ellipse((16,12,size-17,size-21),fill=(218,213,203,255))
    d.line((23,35,42,28),fill=(55,50,45,255),width=4)
    return im


def slider(theme, muted=False, size=(420,72)):
    t=THEMES["muted"] if muted else theme
    im=Image.new("RGBA",size,(0,0,0,0)); w,h=size
    add_shadow(im,(12,18,w-13,h-17),8,alpha=130,blur=6,offset=(0,4))
    d=ImageDraw.Draw(im,"RGBA")
    d.rounded_rectangle((10,15,w-11,h-20),radius=8,fill=(11,12,12,255),outline=t["frame"],width=3)
    d.rounded_rectangle((22,27,w-23,h-32),radius=5,fill=(37,38,37,255),outline=(255,255,255,35),width=1)
    d.ellipse((24,24,42,42),fill=t["accent"]+(255,),outline=(255,255,255,120),width=1)
    d.ellipse((w-43,24,w-25,42),fill=t["accent"]+(255,),outline=(255,255,255,120),width=1)
    return im


def save(im, name):
    # Real raster PNG, palette-quantized only where it does not destroy alpha.
    p=OUT/name
    im.save(p,format="PNG",optimize=True)


for name in ("ivory","studio"):
    th=THEMES[name]
    save(brushed_panel(th),(name+"_panel.png"))
    save(graph_bezel(th),(name+"_graph.png"))
    save(knob_strip(name),name+"_knob_strip.png")
    for st in ("off","on","pressed"):
        save(button(th,st),f"{name}_button_{st}.png")
    save(led(th,False),f"{name}_led_off.png")
    save(led(th,True),f"{name}_led_on.png")
    save(power(th,False),f"{name}_power_off.png")
    save(power(th,True),f"{name}_power_on.png")
    save(screw(th),f"{name}_screw.png")
    save(slider(th),f"{name}_slider.png")

muted=THEMES["muted"]
save(brushed_panel(muted,bypass=True),"muted_panel.png")
save(knob_strip("muted",muted=True),"muted_knob_strip.png")
for st in ("off","on","pressed"):
    save(button(muted,st,muted=True),f"muted_button_{st}.png")
save(led(muted,False,muted=True),"muted_led_off.png")
save(led(muted,True,muted=True),"muted_led_on.png")
save(power(muted,False,muted=True),"muted_power_off.png")
save(power(muted,True,muted=True),"muted_power_on.png")
save(slider(muted,muted=True),"muted_slider.png")
save(led(THEMES["ivory"],True,red=True),"bypass_led_red.png")

manifest = "\n".join(sorted(p.name for p in OUT.glob("*.png"))) + "\n"
(OUT/"MANIFEST.txt").write_text(manifest,encoding="utf-8")
print(f"Generated {len(list(OUT.glob('*.png')))} PNG assets in {OUT}")
