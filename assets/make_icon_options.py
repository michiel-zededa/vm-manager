#!/usr/bin/env python3
"""Generate several app-icon concepts + a contact sheet for choosing one."""
import os, math
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "options"); os.makedirs(OUT, exist_ok=True)
SS = 4
INDIGO=(0x63,0x66,0xF1); VIOLET=(0x8B,0x5C,0xF6); DEEP=(0x4F,0x46,0xE5)
TEAL=(0x14,0xB8,0xA6); SLATE=(0x1E,0x22,0x2B)

def lerp(a,b,t): return tuple(round(a[i]+(b[i]-a[i])*t) for i in range(3))
def rrect_mask(S,r):
    m=Image.new("L",(S,S),0); ImageDraw.Draw(m).rounded_rectangle([0,0,S-1,S-1],radius=r,fill=255); return m
def gradient(S,c0,c1,diag=True):
    img=Image.new("RGB",(S,S)); px=img.load()
    for y in range(S):
        for x in range(S):
            t=((x+y)/(2*(S-1))) if diag else y/(S-1)
            px[x,y]=lerp(c0,c1,t)
    return img
def tile(S,c0,c1):
    t=gradient(S,c0,c1).convert("RGBA"); t.putalpha(rrect_mask(S,int(S*0.225))); return t
def font(px):
    for p in ["/System/Library/Fonts/SFNSRounded.ttf","/System/Library/Fonts/SFNS.ttf",
              "/System/Library/Fonts/Helvetica.ttc"]:
        if os.path.exists(p):
            try: return ImageFont.truetype(p,px)
            except: pass
    return ImageFont.load_default()

def opt_screens(S):  # 1: overlapping screens + play (current)
    t=tile(S,INDIGO,VIOLET); d=ImageDraw.Draw(t)
    w,h,r=int(S*0.46),int(S*0.34),int(S*0.055)
    bx,by=int(S*0.30),int(S*0.26); d.rounded_rectangle([bx,by,bx+w,by+h],r,fill=(255,255,255,70))
    fx,fy=int(S*0.22),int(S*0.40); d.rounded_rectangle([fx,fy,fx+w,fy+h],r,fill=(255,255,255,255))
    dr=int(S*0.014); cy=fy+int(h*0.16)
    for i,c in enumerate([(0xEF,0x44,0x44),(0xF5,0x9E,0x0B),(0x22,0xC5,0x5E)]):
        cx=fx+int(w*0.12)+i*int(dr*3.2); d.ellipse([cx-dr,cy-dr,cx+dr,cy+dr],fill=c+(255,))
    tcx,tcy=fx+w//2,fy+int(h*0.62); ts=int(S*0.05)
    d.polygon([(tcx-ts//2,tcy-ts),(tcx-ts//2,tcy+ts),(tcx+ts,tcy)],fill=INDIGO+(255,))
    return t

def opt_grid(S):  # 2: 2x2 instances, one active
    t=tile(S,INDIGO,DEEP); d=ImageDraw.Draw(t)
    g=int(S*0.10); cell=int(S*0.30); r=int(S*0.05); x0=int(S*0.20); y0=int(S*0.20)
    for i in range(2):
        for j in range(2):
            x=x0+j*(cell+g); y=y0+i*(cell+g)
            active=(i==0 and j==0)
            d.rounded_rectangle([x,y,x+cell,y+cell],r,
                fill=(255,255,255,255) if active else (255,255,255,60))
            if active:
                dd=int(S*0.02); d.ellipse([x+cell-dd*2-int(S*0.03),y+int(S*0.03),
                                           x+cell-int(S*0.03),y+int(S*0.03)+dd*2],fill=(0x22,0xC5,0x5E,255))
    return t

def opt_hex(S):  # 3: hexagon (virtualization core)
    t=tile(S,VIOLET,INDIGO); d=ImageDraw.Draw(t)
    def hexpts(cx,cy,rad):
        return [(cx+rad*math.cos(math.radians(60*k-90)),cy+rad*math.sin(math.radians(60*k-90))) for k in range(6)]
    cx=cy=S//2
    d.polygon(hexpts(cx,cy,int(S*0.30)),fill=(255,255,255,255))
    d.polygon(hexpts(cx,cy,int(S*0.17)),fill=VIOLET+(255,))
    return t

def opt_stack(S):  # 4: server/host stack
    t=tile(S,SLATE,INDIGO); d=ImageDraw.Draw(t)
    w=int(S*0.56); x=int(S*0.22); h=int(S*0.13); r=int(S*0.03); gap=int(S*0.06)
    y=int(S*0.24)
    for i in range(3):
        d.rounded_rectangle([x,y,x+w,y+h],r,fill=(255,255,255,235))
        dd=int(S*0.018); d.ellipse([x+int(S*0.05),y+h//2-dd,x+int(S*0.05)+2*dd,y+h//2+dd],
                                   fill=(0x22,0xC5,0x5E,255) if i<2 else (0xF5,0x9E,0x0B,255))
        y+=h+gap
    return t

def opt_mono(S):  # 5: VM monogram
    t=tile(S,INDIGO,VIOLET); d=ImageDraw.Draw(t)
    f=font(int(S*0.42)); txt="VM"
    bb=d.textbbox((0,0),txt,font=f); tw=bb[2]-bb[0]; th=bb[3]-bb[1]
    d.text(((S-tw)/2-bb[0],(S-th)/2-bb[1]),txt,font=f,fill=(255,255,255,255))
    return t

def opt_cube(S):  # 6: isometric cube (virtual box)
    t=tile(S,INDIGO,VIOLET); d=ImageDraw.Draw(t)
    cx,cy=S//2,int(S*0.52); w=int(S*0.26); hh=int(S*0.15)
    top=[(cx,cy-hh*2),(cx+w,cy-hh),(cx,cy),(cx-w,cy-hh)]
    left=[(cx-w,cy-hh),(cx,cy),(cx,cy+hh*2),(cx-w,cy+hh)]
    right=[(cx+w,cy-hh),(cx,cy),(cx,cy+hh*2),(cx+w,cy+hh)]
    d.polygon(top,fill=(255,255,255,255))
    d.polygon(left,fill=(255,255,255,150))
    d.polygon(right,fill=(255,255,255,90))
    return t

CONCEPTS=[("screens",opt_screens),("grid",opt_grid),("hex",opt_hex),
          ("stack",opt_stack),("mono",opt_mono),("cube",opt_cube)]

def build(fn,size):
    return fn(size*SS).resize((size,size),Image.LANCZOS)

def main():
    for name,fn in CONCEPTS:
        build(fn,512).save(os.path.join(OUT,f"{name}.png"))
    # contact sheet: 3 cols x 2 rows with labels
    cell=300; pad=28; cols=3; rows=2; labelh=44
    W=cols*cell+(cols+1)*pad; H=rows*(cell+labelh)+(rows+1)*pad
    sheet=Image.new("RGB",(W,H),(0x0F,0x11,0x17)); d=ImageDraw.Draw(sheet); f=font(30)
    for idx,(name,fn) in enumerate(CONCEPTS):
        r=idx//cols; c=idx%cols
        x=pad+c*(cell+pad); y=pad+r*(cell+labelh+pad)
        ic=build(fn,cell); sheet.paste(ic,(x,y),ic)
        label=f"{idx+1}. {name}"
        bb=d.textbbox((0,0),label,font=f); tw=bb[2]-bb[0]
        d.text((x+(cell-tw)/2,y+cell+8),label,font=f,fill=(0xE6,0xE9,0xEF))
    sheet.save(os.path.join(HERE,"icon_options.png"))
    print("wrote", os.path.join(HERE,"icon_options.png"))

if __name__=="__main__": main()
