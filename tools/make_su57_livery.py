"""Hand-drawn Su-57 054 reference paint in the source GLB X/Z coordinates.
Run with Python, Pillow and NumPy; no reference images needed at runtime.
"""
from pathlib import Path
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFont
OUT=Path(__file__).resolve().parents[1]/'asset'/'texture'
W,H=3072,2048
XMIN,XMAX=-.7601379156,.4355596304
ZMIN,ZMAX=-.3824476600,.3823808134
PALE=(200,203,193)
BLUE=(65,83,94)
TIP=(113,153,160)
def point(x,z):
    return ((x-XMIN)/(XMAX-XMIN)*(W-1),(ZMAX-z)/(ZMAX-ZMIN)*(H-1))
def polygon(d,ps,c): d.polygon([point(*p) for p in ps],fill=c)
def line(d,ps,c,width=1): d.line([point(*p) for p in ps],fill=c,width=width)
def star(d,x,z,r=.022):
    def shape(s):
        return [point(x+math.cos(math.pi+i*math.pi/5)*s*(1 if i%2==0 else .43),z+math.sin(math.pi+i*math.pi/5)*s*(1 if i%2==0 else .43)) for i in range(10)]
    for s,c in [(r*1.13,(50,58,61)),(r*1.02,(233,231,216)),(r*.79,(165,31,45))]: d.polygon(shape(s),fill=c)
# Measured longitudinal stations of the blue field: explicit pixel steps.
TOP=[(-.70,.003),(-.65,.018),(-.62,.030),(-.60,.024),(-.58,.042),(-.55,.035),(-.52,.048),(-.48,.043),(-.45,.061),(-.42,.054),(-.39,.068),(-.36,.081),(-.33,.073),(-.30,.102),(-.27,.089),(-.24,.119),(-.21,.105),(-.18,.140),(-.15,.126),(-.12,.158),(-.09,.143),(-.06,.185),(-.03,.169),(.00,.209),(.025,.193),(.05,.241),(.075,.225),(.10,.278),(.125,.310),(.15,.297),(.175,.280),(.20,.257),(.225,.214),(.25,.170),(.28,.127),(.31,.111),(.34,.091),(.38,.060),(.43,.012)]
BOTTOM=[(-.70,.005),(-.65,.019),(-.62,.034),(-.59,.025),(-.56,.042),(-.53,.031),(-.50,.052),(-.47,.041),(-.44,.061),(-.41,.052),(-.38,.068),(-.35,.079),(-.32,.073),(-.29,.092),(-.26,.109),(-.23,.096),(-.20,.123),(-.17,.108),(-.14,.142),(-.11,.122),(-.08,.170),(-.05,.158),(-.02,.194),(.01,.180),(.04,.224),(.07,.209),(.10,.259),(.13,.283),(.16,.270),(.19,.248),(.22,.208),(.25,.158),(.28,.141),(.31,.116),(.35,.087),(.43,.009)]
def make_plan(top):
    img=Image.new('RGB',(W,H),PALE); d=ImageDraw.Draw(img)
    edge=TOP if top else BOTTOM
    for s in (-1,1):
        # Trace the boundary as narrow spanwise rows. The reference's teeth
        # are long fore/aft bars, not large square blocks.
        jitter=[0,-.018,.026,-.034,.012,.039,-.016,.007,-.041,.023,-.009,.035]
        for row in range(46):
            z=row*.0075
            crossings=[x for x,v in edge if v>=z]
            if not crossings: continue
            front=min(crossings)+jitter[row%len(jitter)]
            rear=max(crossings)+jitter[(row+5)%len(jitter)]*.35
            polygon(d,[(front,s*z),(rear,s*z),(rear,s*(z+.0075)),(front,s*(z+.0075))],BLUE)
            if row>6 and row%5==2:
                polygon(d,[(front-.040,s*z),(front-.014,s*z),(front-.014,s*(z+.006)),(front-.040,s*(z+.006))],BLUE)
        polygon(d,[(.052,s*.359),(.24,s*.359),(.26,s*.40),(.052,s*.40)],TIP)
        seams=[[(-.40,.10),(.085,.365),(.21,.365)],[(-.34,.111),(.087,.344),(.205,.344)],[(.105,.145),(.105,.298),(.18,.333)],[(.208,.13),(.208,.31)],[(.28,.105),(.36,.249),(.405,.242)],[(.28,.105),(.30,.218),(.405,.242)],[(-.28,.082),(.20,.082)],[(-.31,.037),(-.14,.037),(-.14,.07),(-.31,.07),(-.31,.037)]]
        for ps in seams: line(d,[(x,s*z) for x,z in ps],(102,117,119))
        for x in (-.22,-.08,.07,.22): line(d,[(x,s*.016),(x,s*.069)],(85,101,109))
        star(d,.128,s*.326)
        for i in range(13):
            x=.05+i*.0022; line(d,[(x,s*.077),(x,s*.099)],(30,39,43),2)
        for x,z in [(-.32,.07),(-.17,.11),(.025,.13),(.20,.29)]:
            a,b=point(x,s*z); d.rectangle((a-2,b-1,a+2,b+1),fill=(197,153,61))
    a=np.asarray(img).astype(np.float32)
    rng=np.random.default_rng(5401 if top else 5402)
    yy,xx=np.mgrid[:H,:W]
    a+=rng.normal(0,.65,(H,W,1))+(np.sin(xx*.014)+np.sin(yy*.021+xx*.004))[:,:,None]*.65
    Image.fromarray(np.clip(a,0,255).astype(np.uint8)).save(OUT/('su57_top.bmp' if top else 'su57_bottom.bmp'))
def make_side():
    w,h=2048,1024; img=Image.new('RGB',(w,h),BLUE); d=ImageDraw.Draw(img)
    def p(x,y): return ((x*.85+.55)*w,(1-(y*5+.05))*h)
    def poly(ps,c): d.polygon([p(*a) for a in ps],fill=c)
    poly([(-.8,0),(.5,0),(.5,.033),(-.8,.033)],PALE)
    for i in range(24):
        x=-.65+i*.041; y=[.026,.036,.029,.045,.034,.024][i%6]
        poly([(x,0),(x+.031,0),(x+.031,y),(x,y)],PALE)
    poly([(.06,.135),(.12,.20),(.35,.20),(.35,.145),(.28,.145),(.28,.129),(.25,.129),(.25,.154),(.22,.154),(.22,.142),(.19,.142),(.19,.169),(.16,.169),(.16,.155),(.13,.155),(.13,.135)],PALE)
    try: font=ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf',38)
    except OSError: font=ImageFont.load_default()
    d.text(p(-.462,.104),'054',font=font,anchor='mm',fill=BLUE,stroke_width=1,stroke_fill=(155,165,160))
    img.save(OUT/'su57_side.bmp')
if __name__=='__main__':
    OUT.mkdir(parents=True,exist_ok=True)
    make_plan(True); make_plan(False); make_side()
    print('Wrote Su-57 054 top, underside and side livery')
