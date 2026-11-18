#!/usr/bin/env python3
"""The installer's icon and splash screens, in the Homebrew Menu's look (Inter, from ../libwup/fonts):
    python3 art/make.py      -> icon.png (128x128), tv-splash.png (1280x720), drc-splash.png (854x480)
The icon is a full square (the Wii U Menu frames it and rounds its corners): white "HB" on the Homebrew Menu's ink.
The splash screens show the same mark, rounded as the Wii U Menu shows it. wut's build picks the files up."""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
FONTS = os.path.join(os.environ.get('LIBWUP') or os.path.join(os.path.dirname(ROOT), 'libwup'), 'fonts')
INK, SUBTLE = (0x20, 0x21, 0x24), (0x6b, 0x6e, 0x75)
S = 4                                                       # drawn 4x, scaled down: smooth edges


def mark(d, cx, cy, size, round_corners):
    """the HB mark: white HB on the menu's ink, a square of size px (rounded as the Wii U Menu shows it)"""
    h = size / 2
    d.rounded_rectangle((cx - h, cy - h, cx + h, cy + h), radius=size * 0.14 if round_corners else 0, fill=INK)
    f = ImageFont.truetype(os.path.join(FONTS, 'Inter-Bold.ttf'), round(size * 0.44))
    d.text((cx, cy), 'HB', font=f, fill=(255, 255, 255), anchor='mm')


def icon():
    im = Image.new('RGB', (128 * S, 128 * S), INK)
    mark(ImageDraw.Draw(im), 64 * S, 64 * S, 128 * S, False)
    return im.resize((128, 128), Image.LANCZOS)


def splash(w, h):
    im = Image.new('RGB', (w * S, h * S), (255, 255, 255))
    d = ImageDraw.Draw(im)
    u = h / 480 * S                                         # (laid out for 854x480, scaled)
    mark(d, w * S / 2, h * S / 2 - 70 * u, 96 * u, True)
    big = ImageFont.truetype(os.path.join(FONTS, 'Inter-SemiBold.ttf'), round(34 * u))
    small = ImageFont.truetype(os.path.join(FONTS, 'Inter-Medium.ttf'), round(20 * u))
    d.text((w * S / 2, h * S / 2 + 30 * u), 'Homebrew Menu', font=big, fill=INK, anchor='mm')
    d.text((w * S / 2, h * S / 2 + 72 * u), 'Installer', font=small, fill=SUBTLE, anchor='mm')
    return im.resize((w, h), Image.LANCZOS)


if __name__ == '__main__':
    icon().save(os.path.join(ROOT, 'icon.png'))
    splash(1280, 720).save(os.path.join(ROOT, 'tv-splash.png'))
    splash(854, 480).save(os.path.join(ROOT, 'drc-splash.png'))
    print('icon.png, tv-splash.png, drc-splash.png')
