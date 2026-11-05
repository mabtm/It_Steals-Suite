"""Build Suite artwork from editable vectors and actual UI captures.

Requires Pillow and CairoSVG (or Inkscape). Run tests/render_ui.py first, then:
python assets/build.py --captures build/verification/ui_tests/work/draw
Without --captures, only Windows icon/installer artwork is regenerated.
"""

import argparse
import shutil
import subprocess
import tempfile
from io import BytesIO
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parent
BACKGROUND = "#0e1116"
PANEL = "#151c26"
OUTLINE = "#304052"
MINT = "#7ad4ff"
WHITE = "#f0f5fa"
MUTED = "#97a8bb"


def font(size, bold=False):
    name = "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"
    return ImageFont.truetype(name, size)


def vector(path, size):
    try:
        import cairosvg
    except ImportError:
        if not shutil.which("inkscape"):
            raise RuntimeError(
                "Install CairoSVG or Inkscape to render the logo"
            ) from None
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "vector.png"
            subprocess.run(
                [
                    "inkscape",
                    str(path),
                    "--export-type=png",
                    f"--export-filename={output}",
                    f"--export-width={size}",
                    f"--export-height={size}",
                ],
                check=True,
                capture_output=True,
            )
            return Image.open(output).convert("RGBA")
    raw = cairosvg.svg2png(url=str(path), output_width=size, output_height=size)
    return Image.open(BytesIO(raw)).convert("RGBA")


def windows_artwork():
    logo = vector(ROOT / "logo.svg", 512)
    logo.save(
        ROOT / "suite.ico",
        sizes=[(n, n) for n in (16, 20, 24, 32, 40, 48, 64, 128, 256)],
    )
    sidebar = Image.new("RGB", (328, 628), PANEL)
    draw = ImageDraw.Draw(sidebar)
    for x in range(0, 328, 32):
        draw.line((x, 0, x, 628), fill="#17292c")
    for y in range(0, 628, 32):
        draw.line((0, y, 328, y), fill="#17292c")
    mark = logo.resize((188, 188), Image.Resampling.LANCZOS)
    sidebar.paste(mark, (39, 69), mark)
    draw.text((30, 303), "ELITE", font=font(32, True), fill=WHITE)
    draw.text((30, 345), "SUITE", font=font(32, True), fill=MINT)
    draw.line((30, 416, 289, 416), fill=OUTLINE, width=2)
    for index, text in enumerate(("Practice.", "Understand.", "Improve.")):
        draw.text(
            (30, 444 + index * 37),
            text,
            font=font(24),
            fill=MINT if index == 2 else MUTED,
        )
    sidebar.save(ROOT / "setup-sidebar.bmp")
    header = Image.new("RGB", (300, 114), PANEL)
    small = logo.resize((82, 82), Image.Resampling.LANCZOS)
    header.paste(small, (200, 16), small)
    ImageDraw.Draw(header).text(
        (14, 43), "ELITE-SUITE", font=font(16, True), fill=MINT
    )
    header.save(ROOT / "setup-header.bmp")
    return logo


def capture(folder, name, box=None):
    path = folder / (name + ".png")
    with Image.open(path) as source:
        return source.convert("RGB").crop(box) if box else source.convert("RGB")


def fit(image, size):
    result = Image.new("RGB", size, BACKGROUND)
    scaled = ImageOps.contain(image, size, Image.Resampling.LANCZOS)
    result.paste(
        scaled, ((size[0] - scaled.width) // 2, (size[1] - scaled.height) // 2)
    )
    return result


def paste_rounded(canvas, source, box, radius=16):
    x, y, width, height = box
    source = fit(source, (width, height))
    mask = Image.new("L", (width, height))
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, width - 1, height - 1), radius, fill=255
    )
    canvas.paste(source, (x, y), mask)


def repository_artwork(folder, logo):
    # Crop only empty surrounding space or focus on a real UI region; never paint over UI values.
    practice = capture(folder, "tour-01-tools", (100, 80, 1240, 640))
    search = capture(folder, "tour-02-search-7", (334, 194, 1222, 984))
    replay = capture(folder, "tour-03-replay-7", (474, 124, 1662, 1054))
    clip = capture(folder, "751-studio-0", (5, 5, 1910, 1075))
    inspect = capture(folder, "7115-summary-1920-dpi-100", (254, 186, 1122, 700))
    route = capture(folder, "seed-route-full", (430, 136, 850, 436))
    items = [
        ("Practice", "Explore layouts. Inspect the details.", practice),
        ("Routing", "Search seeds. Find your next route.", search),
        ("Replay", "Review the run, one move at a time.", replay),
        ("Clip Studio", "Shape the camera. Export the clip.", clip),
    ]
    cards = Image.new("RGB", (1280, 1044), BACKGROUND)
    draw = ImageDraw.Draw(cards)
    for index, (title, detail, image) in enumerate(items):
        x, y = 16 + (index % 2) * 632, 16 + (index // 2) * 500
        draw.rounded_rectangle(
            (x, y, x + 616, y + 484), 20, fill=PANEL, outline=OUTLINE, width=2
        )
        draw.text((x + 24, y + 20), f"0{index + 1}", font=font(16, True), fill=MINT)
        draw.text((x + 68, y + 15), title, font=font(31, True), fill=WHITE)
        paste_rounded(cards, image, (x + 18, y + 70, 580, 340))
        draw.text((x + 24, y + 434), detail, font=font(22), fill=MUTED)
    draw.text((28, 1012), "INTERFACE PREVIEW / TEST DATA", font=font(16), fill=MUTED)
    cards.save(ROOT / "features.png", optimize=True)

    comparison = Image.new("RGB", (1280, 570), BACKGROUND)
    draw = ImageDraw.Draw(comparison)
    draw.text((28, 22), "FROM A LAYOUT TO THE DETAILS", font=font(23, True), fill=WHITE)
    draw.text((28, 58), "INTERFACE PREVIEW / TEST DATA", font=font(17), fill=MUTED)
    for index, (title, image) in enumerate(
        (
            ("Plan the route", route),
            ("Review the run", replay),
            ("Inspect an object", inspect),
        )
    ):
        x = 20 + index * 420
        draw.rounded_rectangle((x, 106, x + 400, 548), 16, fill=PANEL, outline=OUTLINE)
        paste_rounded(comparison, image, (x + 12, 118, 376, 360))
        draw.text((x + 18, 500), title, font=font(23, True), fill=MINT)
    comparison.save(ROOT / "comparison.png", optimize=True)

    # A clearly labelled slide tour, not simulated gameplay or a performance demonstration.
    slides = []
    for index, (title, detail, image) in enumerate(items):
        slide = Image.new("RGB", (1120, 710), BACKGROUND)
        d = ImageDraw.Draw(slide)
        d.text((28, 20), "ELITE-SUITE", font=font(17, True), fill=MINT)
        d.text((28, 51), title, font=font(32, True), fill=WHITE)
        d.text((710, 62), "INTERFACE PREVIEW / TEST DATA", font=font(16), fill=MUTED)
        paste_rounded(slide, image, (20, 108, 1080, 530))
        d.text((28, 665), detail, font=font(20), fill=MUTED)
        for chapter in range(4):
            x = 934 + chapter * 37
            d.rounded_rectangle(
                (x, 674, x + 26, 679), 2, fill=MINT if chapter == index else OUTLINE
            )
        slides.append(slide)
    frames, durations = [], []
    for index, slide in enumerate(slides):
        frames.append(slide)
        durations.append(2500)
        following = slides[(index + 1) % len(slides)]
        for amount in (0.25, 0.5, 0.75):
            frames.append(Image.blend(slide, following, amount))
            durations.append(90)
    frames[0].save(
        ROOT / "showcase.gif",
        save_all=True,
        append_images=frames[1:],
        duration=durations,
        loop=0,
        optimize=True,
    )

    social = Image.new("RGB", (1280, 640), BACKGROUND)
    draw = ImageDraw.Draw(social)
    for x in range(0, 1280, 40):
        draw.line((x, 0, x, 640), fill="#11222a")
    for y in range(0, 640, 40):
        draw.line((0, y, 1280, y), fill="#11222a")
    mark = logo.resize((110, 110), Image.Resampling.LANCZOS)
    social.paste(mark, (40, 40), mark)
    draw.text((40, 178), "IT STEALS", font=font(22), fill=MUTED)
    draw.text((38, 219), "ELITE", font=font(54, True), fill=WHITE)
    draw.text((38, 280), "SUITE", font=font(54, True), fill=MINT)
    for index, text in enumerate(("Practice.", "Understand.", "Improve.")):
        draw.text((42, 379 + index * 46), text, font=font(32), fill=WHITE)
    paste_rounded(social, search, (570, 32, 315, 450))
    paste_rounded(social, clip, (905, 32, 340, 265))
    paste_rounded(social, replay, (905, 317, 340, 285))
    draw.text((575, 523), "Seeds. Routes.", font=font(22, True), fill=MINT)
    draw.text((575, 556), "Replays. Clips.", font=font(22, True), fill=MINT)
    draw.text((575, 603), "INTERFACE PREVIEW / TEST DATA", font=font(16), fill=MUTED)
    social_path = ROOT.parent / "build" / "branding" / "social-preview.png"
    social_path.parent.mkdir(parents=True, exist_ok=True)
    social.save(social_path, optimize=True)
    if social_path.stat().st_size >= 1_000_000:
        raise ValueError("Social preview exceeds GitHub's size limit")
    print("Repository artwork generated; social preview saved to build/branding/.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path)
    args = parser.parse_args()
    logo = windows_artwork()
    if args.captures:
        repository_artwork(args.captures, logo)
    print("Windows icon and installer artwork generated.")


if __name__ == "__main__":
    main()
