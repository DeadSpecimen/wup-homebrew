#!/usr/bin/env python3
"""The apps' icons (64x64, in the style of the other apps': a full square, a white symbol), from Font Awesome 5
(SIL OFL 1.1), the copy LVGL ships (../../libwup/third_party/lvgl/scripts/built_in_font).
    python3 apps/mkicons.py     -> <app>/icon.png"""
import os
from PIL import Image, ImageDraw, ImageFont
HERE = os.path.dirname(os.path.abspath(__file__))
FA = os.path.join(os.environ.get('LIBWUP') or os.path.join(HERE, '..', '..', 'libwup'), 'third_party', 'lvgl', 'scripts', 'built_in_font', 'FontAwesome5-Solid+Brands+Regular.woff')
S = 4


def icon(path, top, bottom, glyph):
    im = Image.new('RGB', (64 * S, 64 * S)); d = ImageDraw.Draw(im)
    for y in range(64 * S):                          # a gentle vertical gradient
        t = y / (64 * S - 1); d.line([(0, y), (64 * S, y)], fill=tuple(int(a + (b - a) * t) for a, b in zip(top, bottom)))
    f = ImageFont.truetype(FA, 34 * S)
    d.text((32 * S, 33 * S), glyph, font=f, fill=(255, 255, 255), anchor='mm')
    im.resize((64, 64), Image.LANCZOS).save(path)


if __name__ == '__main__':
    icon(os.path.join(HERE, 'app_store', 'icon.png'), (0x2b, 0x8a, 0xf7), (0x14, 0x5f, 0xd6), '')        # shopping bag
    icon(os.path.join(HERE, 'software_update', 'icon.png'), (0x6b, 0x71, 0x7a), (0x3c, 0x40, 0x46), '')  # arrows going round
    print('icons made')
