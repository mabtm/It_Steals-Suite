#!/usr/bin/env python3
"""Regenerate committed Windows branding: pip install Pillow CairoSVG."""

from io import BytesIO
from pathlib import Path
import cairosvg
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parent
logo = cairosvg.svg2png(url=str(root / "logo.svg"), output_width=512, output_height=512)
image = Image.open(BytesIO(logo)).convert("RGBA")
image.save(
    root / "suite.ico", sizes=[(n, n) for n in (16, 20, 24, 32, 40, 48, 64, 128, 256)]
)


def font(size, bold=False):
    family = "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"
    try:
        return ImageFont.truetype(family, size)
    except OSError:
        return ImageFont.load_default(size=size)


sidebar = Image.new("RGB", (328, 628), "#101b20")
draw = ImageDraw.Draw(sidebar)
for x in range(0, 328, 32):
    draw.line((x, 0, x, 628), fill="#17292c")
for y in range(0, 628, 32):
    draw.line((0, y, 328, y), fill="#17292c")
sidebar.paste(
    image.resize((188, 188), Image.Resampling.LANCZOS),
    (39, 69),
    image.resize((188, 188), Image.Resampling.LANCZOS),
)
draw.text((30, 303), "PRACTICE", font=font(32, True), fill="#eff8f4")
draw.text((30, 345), "SUITE", font=font(32, True), fill="#8ee5d1")
draw.line((30, 416, 289, 416), fill="#36524b", width=2)
draw.text((30, 443), "Ready for your", font=font(22), fill="#abc4bb")
draw.text((30, 476), "next run.", font=font(22), fill="#abc4bb")
draw.text((30, 573), "BY MAB", font=font(17, True), fill="#7ab2a2")
sidebar.save(root / "setup-sidebar.bmp")
header = Image.new("RGB", (300, 114), "#101b20")
small = image.resize((82, 82), Image.Resampling.LANCZOS)
header.paste(small, (200, 16), small)
ImageDraw.Draw(header).text(
    (14, 43), "PRACTICE SUITE", font=font(16, True), fill="#a0dfcc"
)
header.save(root / "setup-header.bmp")
print("Windows icon and installer artwork generated.")
