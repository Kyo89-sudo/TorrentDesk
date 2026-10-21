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

# Command icons: simple flat glyphs on a transparent background, filled with
# the Flow logo gradient (blue -> cyan), so they match the app icon while
# sitting quietly on native push buttons. Shapes are drawn as a mask first.
INK = (255, 255, 255, 255)
def flow_fill(mask):
    im = Image.new('RGBA', (256, 256))
    d = ImageDraw.Draw(im)
    a, b = (0, 96, 240), (0, 178, 228)  # darker than the logo for contrast on light buttons
    for i in range(512):
        t = i / 511
        c = tuple(int(x + (y - x) * t) for x, y in zip(a, b)) + (255,)
        d.line((i - 256, 256, i, 0), fill=c, width=2)  # diagonal, bottom-left -> top-right
    im.putalpha(mask.getchannel('A'))
    return im
W = 22  # stroke width at 256 px (~1.7 px at 20 px)
def glyph(name):
    im = Image.new('RGBA', (256, 256), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    def line(*pts):
        d.line(pts, fill=INK, width=W, joint='curve')
        for x, y in (pts[0], pts[-1]):
            d.ellipse((x - W/2, y - W/2, x + W/2, y + W/2), fill=INK)
    if name == 'add':
        line((128, 48), (128, 208)); line((48, 128), (208, 128))
    elif name == 'start':
        d.polygon([(76, 40), (212, 128), (76, 216)], fill=INK)
    elif name == 'pause':
        d.rectangle((66, 48, 106, 208), fill=INK); d.rectangle((150, 48, 190, 208), fill=INK)
    elif name == 'remove':
        line((44, 72), (212, 72)); line((100, 72), (108, 40), (148, 40), (156, 72))
        line((68, 72), (80, 220), (176, 220), (188, 72))
        line((108, 108), (110, 184)); line((148, 108), (146, 184))
    elif name == 'settings':
        pts = []
        for i in range(48):
            a = 2 * math.pi * (i + 0.5) / 48
            r = 112 if (i // 3) % 2 == 0 else 86
            pts.append((128 + math.cos(a) * r, 128 + math.sin(a) * r))
        d.polygon(pts, fill=INK)
        d.ellipse((92, 92, 164, 164), fill=(0, 0, 0, 0))
    elif name == 'tray':
        d.rounded_rectangle((36, 52, 220, 204), radius=18, outline=INK, width=W)
        line((84, 160), (172, 160))
    else:  # help
        d.ellipse((24, 24, 232, 232), outline=INK, width=W)
        d.arc((90, 62, 166, 138), 180, 90, fill=INK, width=W)
        line((128, 138), (128, 156))
        d.ellipse((115, 176, 141, 202), fill=INK)
    return im

for name in ['add', 'start', 'pause', 'remove', 'settings', 'tray', 'help']:
    tile = flow_fill(glyph(name))
    tile.save(OUT / (name + '.ico'), sizes=SIZES)
    tile.resize((64, 64), Image.Resampling.LANCZOS).save(OUT / (name + '.png'))
print('Flow application icon and 7 flat command icons ready')
