"""Encode the approved Flow logo and draw consistent small command icons.
Only the optional asset-generation step needs Pillow; building the app does not.
"""
from PIL import Image, ImageDraw
from pathlib import Path
import math
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'assets'
SIZES = [(16,16),(20,20),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)]
logo = Image.open(OUT / 'flow-logo.png').convert('RGBA')
logo.save(OUT / 'torrentdesk.ico', sizes=SIZES)
logo.resize((256,256), Image.Resampling.LANCZOS).save(OUT / 'torrentdesk.png')

def gradient(top, bottom):
    im = Image.new('RGBA',(256,256))
    d = ImageDraw.Draw(im)
    for y in range(256):
        t=y/255
        d.line((0,y,256,y),fill=tuple(int(a+(b-a)*t) for a,b in zip(top,bottom))+(255,))
    return im

for name in ['add','start','pause','remove','settings','tray','help']:
    tile=gradient((20,48,110),(5,17,43));rounding=Image.new('L',(256,256));ImageDraw.Draw(rounding).rounded_rectangle((4,4,252,252),radius=54,fill=255);tile.putalpha(rounding)
    mask=Image.new('L',(256,256));d=ImageDraw.Draw(mask)
    if name=='add':
        d.rounded_rectangle((110,53,146,203),radius=12,fill=255)
        d.rounded_rectangle((53,110,203,146),radius=12,fill=255)
    elif name=='start':
        d.polygon([(88,52),(205,128),(88,204)],fill=255)
    elif name=='pause':
        d.rounded_rectangle((69,58,108,198),radius=12,fill=255)
        d.rounded_rectangle((148,58,187,198),radius=12,fill=255)
    elif name=='remove':
        d.rounded_rectangle((78,91,178,203),radius=17,fill=255)
        d.rounded_rectangle((64,61,192,82),radius=10,fill=255)
        d.rounded_rectangle((106,44,150,65),radius=10,fill=255)
        d.rounded_rectangle((104,113,117,180),radius=6,fill=0)
        d.rounded_rectangle((140,113,153,180),radius=6,fill=0)
    elif name=='settings':
        points=[]
        for i in range(32):
            a=2*math.pi*i/32;radius=83 if i%4 in (1,2) else 68
            points.append((128+math.cos(a)*radius,128+math.sin(a)*radius))
        d.polygon(points,fill=255);d.ellipse((93,93,163,163),fill=0)
    elif name=='tray':
        d.rounded_rectangle((112,46,144,145),radius=10,fill=255)
        d.polygon([(73,116),(128,174),(183,116)],fill=255)
        d.rounded_rectangle((56,188,200,210),radius=10,fill=255)
    else:
        d.ellipse((111,48,145,82),fill=255)
        d.rounded_rectangle((112,102,144,204),radius=13,fill=255)
    ink=gradient((91,241,255),(0,118,255));ink.putalpha(mask);tile.alpha_composite(ink)
    tile.save(OUT / (name+'.ico'),sizes=SIZES)
    tile.resize((64,64),Image.Resampling.LANCZOS).save(OUT/(name+'.png'))
print('Flow application icon and 7 command icons ready')
