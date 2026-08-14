#!/usr/bin/env python3
"""Hex + stack hybrids: a hexagon 'core' holding stacked hosts. Four treatments."""
import os, math
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "options"); os.makedirs(OUT, exist_ok=True)
SS = 4
INDIGO=(0x63,0x66,0xF1); VIOLET=(0x8B,0x5C,0xF6); DEEP=(0x43,0x38,0xCA); SLATE=(0x20,0x24,0x2E)
GREEN=(0x22,0xC5,0x5E); AMBER=(0xF5,0x9E,0x0B)

def lerp(a,b,t): return tuple(round(a[i]+(b[i]-a[i])*t) for i in range(3))
def rrect_mask(S,r):
    m=Image.new("L",(S,S),0); ImageDraw.Draw(m).rounded_rectangle([0,0,S-1,S-1],radius=r,fill=255); return m
def gradient(S,c0,c1):
    img=Image.new("RGB",(S,S)); px=img.load()
    for y in range(S):
        for x in range(S):
            px[x,y]=lerp(c0,c1,(x+y)/(2*(S-1)))
    return img
def tile(S,c0,c1):
    t=gradient(S,c0,c1).convert("RGBA"); t.putalpha(rrect_mask(S,int(S*0.225))); return t
def hexpts(cx,cy,R,flat=True):
    off = 0 if flat else -90
    return [(cx+R*math.cos(math.radians(off+60*k)), cy+R*math.sin(math.radians(off+60*k))) for k in range(6)]
def hex_mask(S,cx,cy,R,flat=True):
    m=Image.new("L",(S,S),0); ImageDraw.Draw(m).polygon(hexpts(cx,cy,R,flat),fill=255); return m

def bars_layer(S, cx, cy, R, color, dots=True, dotcols=(GREEN,GREEN,AMBER)):
    """Three rounded horizontal bars centred in the hex, later clipped to it."""
    layer=Image.new("RGBA",(S,S),(0,0,0,0)); d=ImageDraw.Draw(layer)
    bw=int(R*1.7); bh=int(R*0.26); gap=int(R*0.16); r=int(bh*0.42)
    x0=cx-bw//2; total=3*bh+2*gap; y=cy-total//2
    for i in range(3):
        d.rounded_rectangle([x0,y,x0+bw,y+bh],r,fill=color+(255,))
        if dots:
            dr=int(bh*0.17); dcx=x0+int(bh*0.7); dcy=y+bh//2
            dc=dotcols[i]
            d.ellipse([dcx-dr,dcy-dr,dcx+dr,dcy+dr],fill=dc+(255,))
        y+=bh+gap
    return layer

def compose(S, tilecols, hex_fill=None, hex_ring=None, bar_color=SLATE, dotcols=(GREEN,GREEN,AMBER)):
    t=tile(S,*tilecols); d=ImageDraw.Draw(t)
    cx=cy=S//2; R=int(S*0.34)
    if hex_fill is not None:
        d.polygon(hexpts(cx,cy,R), fill=hex_fill+(255,))
    if hex_ring is not None:
        d.line(hexpts(cx,cy,R)+[hexpts(cx,cy,R)[0]], fill=hex_ring+(255,), width=int(S*0.028), joint="curve")
    # bars clipped to hex
    bars=bars_layer(S,cx,cy,int(R*0.92),bar_color,dotcols=dotcols)
    hm=hex_mask(S,cx,cy,int(R*0.92))
    clipped=Image.new("RGBA",(S,S),(0,0,0,0)); clipped.paste(bars,(0,0),Image.composite(bars.split()[3],Image.new("L",(S,S),0),hm))
    t=Image.alpha_composite(t,clipped)
    return t

def A(S):  # white hex, dark bars + status dots
    return compose(S,(INDIGO,VIOLET),hex_fill=(255,255,255),bar_color=SLATE)
def B(S):  # gradient tile, white hex ring, white bars
    return compose(S,(VIOLET,INDIGO),hex_ring=(255,255,255),bar_color=(255,255,255),dotcols=(INDIGO,INDIGO,INDIGO))
def C(S):  # white hex, indigo bars (brand), no dots-as-status but indigo
    return compose(S,(INDIGO,DEEP),hex_fill=(255,255,255),bar_color=INDIGO,dotcols=(GREEN,GREEN,GREEN))
def D(S):  # deep tile, white hex ring + white hex faint fill, dark bars
    t=compose(S,(SLATE,INDIGO),hex_ring=(255,255,255),bar_color=(255,255,255),dotcols=(GREEN,GREEN,AMBER))
    return t

VARS=[("hexstack-a",A),("hexstack-b",B),("hexstack-c",C),("hexstack-d",D)]

def build(fn,size): return fn(size*SS).resize((size,size),Image.LANCZOS)

def main():
    for n,fn in VARS: build(fn,512).save(os.path.join(OUT,n+".png"))
    from PIL import ImageFont
    def font(px):
        for p in ["/System/Library/Fonts/SFNSRounded.ttf","/System/Library/Fonts/Helvetica.ttc"]:
            if os.path.exists(p):
                try: return ImageFont.truetype(p,px)
                except: pass
        return ImageFont.load_default()
    cell=320; pad=28; labelh=44; cols=4
    W=cols*cell+(cols+1)*pad; H=cell+labelh+2*pad
    sheet=Image.new("RGB",(W,H),(0x0F,0x11,0x17)); d=ImageDraw.Draw(sheet); f=font(30)
    for i,(n,fn) in enumerate(VARS):
        x=pad+i*(cell+pad); y=pad; ic=build(fn,cell); sheet.paste(ic,(x,y),ic)
        lab=f"{chr(65+i)}"
        bb=d.textbbox((0,0),lab,font=f); d.text((x+(cell-(bb[2]-bb[0]))/2,y+cell+8),lab,font=f,fill=(0xE6,0xE9,0xEF))
    sheet.save(os.path.join(HERE,"icon_combo.png")); print("wrote icon_combo.png")

if __name__=="__main__": main()
