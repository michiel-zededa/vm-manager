#!/usr/bin/env python3
"""Generate the VM Manager app icon set (design D: an outlined hexagon 'core'
holding stacked hosts with status dots) and emit per-platform assets:
  - assets/icon_1024.png / icon_512.png / icon_256.png
  - assets/AppIcon.icns (macOS, via iconutil)
  - assets/AppIcon.ico  (Windows, multi-size)

Run: python3 assets/make_icon.py
"""
import os, math, subprocess, shutil
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
SS = 4
BASE = 1024
SLATE=(0x20,0x24,0x2E); INDIGO=(0x63,0x66,0xF1)
GREEN=(0x22,0xC5,0x5E); AMBER=(0xF5,0x9E,0x0B); WHITE=(255,255,255)

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
def hexpts(cx,cy,R):
    return [(cx+R*math.cos(math.radians(60*k)), cy+R*math.sin(math.radians(60*k))) for k in range(6)]
def hex_mask(S,cx,cy,R):
    m=Image.new("L",(S,S),0); ImageDraw.Draw(m).polygon(hexpts(cx,cy,R),fill=255); return m

def build(S):
    t=tile(S,SLATE,INDIGO); d=ImageDraw.Draw(t)
    cx=cy=S//2; R=int(S*0.34)
    # Outlined hexagon core
    pts=hexpts(cx,cy,R)
    d.line(pts+[pts[0]], fill=WHITE+(255,), width=max(2,int(S*0.030)), joint="curve")
    # Stacked host bars, clipped to the hexagon interior
    Rin=int(R*0.92)
    layer=Image.new("RGBA",(S,S),(0,0,0,0)); ld=ImageDraw.Draw(layer)
    bw=int(Rin*1.7); bh=int(Rin*0.26); gap=int(Rin*0.16); r=int(bh*0.42)
    x0=cx-bw//2; total=3*bh+2*gap; y=cy-total//2
    for i,dc in enumerate((GREEN,GREEN,AMBER)):
        ld.rounded_rectangle([x0,y,x0+bw,y+bh],r,fill=WHITE+(255,))
        dr=int(bh*0.18); dcx=x0+int(bh*0.72); dcy=y+bh//2
        ld.ellipse([dcx-dr,dcy-dr,dcx+dr,dcy+dr],fill=dc+(255,))
        y+=bh+gap
    hm=hex_mask(S,cx,cy,Rin)
    clip=Image.composite(layer.split()[3],Image.new("L",(S,S),0),hm)
    t.paste(layer,(0,0),clip)
    return t

def render(size):
    return build(size*SS).resize((size,size),Image.LANCZOS)

def main():
    render(BASE).save(os.path.join(HERE,"icon_1024.png"))
    render(512).save(os.path.join(HERE,"icon_512.png"))
    render(256).save(os.path.join(HERE,"icon_256.png"))

    ico_sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)]
    render(256).save(os.path.join(HERE,"AppIcon.ico"), sizes=ico_sizes)

    iconset=os.path.join(HERE,"AppIcon.iconset")
    if os.path.exists(iconset): shutil.rmtree(iconset)
    os.makedirs(iconset)
    for base,scale in [(16,1),(16,2),(32,1),(32,2),(128,1),(128,2),(256,1),(256,2),(512,1),(512,2)]:
        name=f"icon_{base}x{base}{'@2x' if scale==2 else ''}.png"
        render(base*scale).save(os.path.join(iconset,name))
    if shutil.which("iconutil"):
        subprocess.run(["iconutil","-c","icns",iconset,"-o",os.path.join(HERE,"AppIcon.icns")],check=True)
        shutil.rmtree(iconset)
    print("Icon set (design D) written to", HERE)

if __name__=="__main__": main()
