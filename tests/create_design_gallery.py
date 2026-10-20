"""Build a gallery from real headless ImGui captures, not a UI mockup."""
from pathlib import Path
import argparse
import base64
import html
import json
from concurrent.futures import ProcessPoolExecutor
from PIL import Image, ImageDraw, ImageFont
from render_ui import render

BG = (9, 12, 15)
INK = (232, 242, 242)
MUTED = (143, 166, 172)
TEAL = (94, 212, 196)


def raster(path):
    png=path.with_suffix('.png')
    if not png.exists() or png.stat().st_mtime < path.stat().st_mtime:
        render(path)
    return path.with_suffix('.png')


def font(size):
    try:
        return ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', size)
    except OSError:
        return ImageFont.load_default()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('draws', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('exports', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    captures = sorted(args.draws.glob('tour-*.draw'))
    captures.append(args.draws / 'intro-first-visible-presentation.draw')
    assert len(captures) == 44, len(captures)
    with ProcessPoolExecutor(max_workers=2) as pool:
        for path in pool.map(raster, captures):
            print(path.name, flush=True)
    chapters = [
        ('00-intro', 'Intro', 'Die bestehende Startanimation.', ['intro-first-visible-presentation'], (636, 400, 1284, 680)),
        ('01-tools', 'Navigation & Funktionskarten', 'Eigene Vektorsymbole, aktive Akzente und klare Zustände.', ['tour-01-tools'], (96, 76, 1244, 1004)),
        ('02-search', 'Seed-Suche', 'Echte Zähler, Aktivität und kurz hervorgehobene neue Ergebnisse.', [f'tour-02-search-{i}' for i in range(8)], (96, 76, 1244, 1004)),
        ('03-replay', 'Replay & Ereignisse', 'Große Karte und anklickbare Pickup- und Flash-Zeitpunkte.', [f'tour-03-replay-{i}' for i in range(12)], (236, 8, 1684, 1072)),
        ('04-settings', 'Dein Stil', 'Farben und Schrift bleiben direkt anpassbar.', ['tour-04-settings'], (96, 76, 1244, 1004)),
        ('05-diagnostics', 'Diagnostics', 'Eine verständliche Startansicht führt zur bestehenden Prüfung.', ['tour-05-diagnostics'], (96, 76, 1244, 1004)),
        ('06-huds-map', 'HUD-Profile & Kompass', 'Getrennte Profilvorschauen. Minimap im Spiel: Normal / Practice.', ['tour-06-huds-map'], None),
        ('07-pb', 'Gültiger lokaler PB', 'Abzeichen und Differenz erst nach abgeschlossener Prüfung; eigener leiser Ton.', [f'tour-07-pb-{i}' for i in range(10)], (396, 104, 1524, 976)),
        ('08-states', 'Laden & leere Ansichten', 'Dezente Bewegung mit klarer Erklärung des nächsten Schritts.', [f'tour-08-states-{i}' for i in range(8)], (96, 86, 1064, 734)),
    ]
    originals = []
    frames, durations = [], []
    for key, title, detail, names, crop in chapters:
        cropped = []
        for name in names:
            image = Image.open(args.draws / (name + '.png')).convert('RGB')
            if crop:
                image = image.crop(crop)
            else:
                # Arrange three independent native HUD/profile previews for
                # legibility. This does not represent a simultaneous clean-run UI.
                panels = [image.crop((8, 8, 376, 376)), image.crop((1534, 8, 1912, 353)), image.crop((1514, 882, 1912, 1072))]
                image = Image.new('RGB', (1148, 820), BG)
                image.paste(panels[0], (70, 110))
                image.paste(panels[1], (610, 110))
                image.paste(panels[2], (600, 510))
            cropped.append(image)
            frame = Image.new('RGB', (1600, 1240), BG)
            draw = ImageDraw.Draw(frame)
            draw.line((44, 27, 1556, 27), fill=(38, 67, 70), width=1)
            draw.text((44, 38), title, font=font(30), fill=INK)
            draw.text((1290, 44), 'v7.1.0 / mab', font=font(21), fill=TEAL)
            draw.text((44, 82), detail, font=font(19), fill=MUTED)
            scale = min(1, 1512 / image.width, 1056 / image.height)
            display = image.resize((round(image.width * scale), round(image.height * scale)), Image.Resampling.LANCZOS) if scale != 1 else image
            frame.paste(display, ((1600 - display.width) // 2, 126 + (1056 - display.height) // 2))
            draw.text((44, 1202), 'Native ImGui-Ausgabe · synthetische Testdaten · keine Windows-/Unity-Liveaufnahme', font=font(18), fill=MUTED)
            frames.append(frame)
            durations.append(120 if len(names) > 1 else 2600)
        if len(names) > 1:
            durations[-1] += 1900
        representative = cropped[5 if key == '03-replay' else -1]
        representative.save(args.output / (key + '.png'))
        originals.append((key, title, detail, representative))
    # One shared palette avoids frame-to-frame color flicker.
    sample = Image.new('RGB', (768, 768), BG)
    for i, frame in enumerate([frames[0], frames[1], frames[9], frames[15], frames[23], frames[-1], frames[-10], frames[-15], frames[-20]]):
        sample.paste(frame.resize((256, 256)), ((i % 3) * 256, (i // 3) * 256))
    # Preserve small semantic markers even when their pixel area is tiny.
    anchors = [BG, INK, MUTED, TEAL, (106,214,196), (233,119,220),
               (245,198,92), (76,240,102), (87,240,156), (255,92,97),
               (255,204,97), (250,104,91), (12,19,27), (65,86,105),
               (145,86,62), (120,70,50), (100,62,45), (180,100,70), (208,116,81)]
    adaptive = sample.quantize(colors=192).getpalette()[:192*3]
    colors = [channel for rgb in anchors for channel in rgb] + adaptive
    palette = Image.new('P',(1,1))
    palette.putpalette((colors+[0]*768)[:768])
    gif = args.output / 'interface-tour.gif'
    encoded = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
    encoded[0].save(gif, save_all=True, append_images=encoded[1:], duration=durations, loop=0, optimize=False, disposal=2, palette=palette.getpalette())
    with Image.open(gif) as image:
        assert image.n_frames == len(encoded) and image.size == (1600, 1240)
        for i,expected in enumerate(encoded):
            image.seek(i)
            assert image.convert('RGB').tobytes() == expected.convert('RGB').tobytes(), ('GIF color/disposal mismatch',i)
    contact = Image.new('RGB', (3240, 2838), BG)
    draw = ImageDraw.Draw(contact)
    draw.text((40, 25), 'PRACTICE SUITE  /  7.1.0  /  mab', fill=INK, font=font(42))
    draw.text((40, 88), 'Native Oberfläche mit synthetischen Testdaten. Einzelbilder und Animation in der Galerie.', fill=MUTED, font=font(25))
    for i, (_, title, detail, image) in enumerate(originals):
        x, y = 30 + (i % 3) * 1070, 160 + (i // 3) * 884
        draw.rounded_rectangle((x, y, x + 1040, y + 854), radius=14, outline=(44, 67, 75), width=2)
        draw.text((x + 20, y + 18), title, fill=TEAL, font=font(25))
        scale = min(1000 / image.width, 780 / image.height)
        view = image.resize((round(image.width * scale), round(image.height * scale)), Image.Resampling.LANCZOS)
        contact.paste(view, (x + (1040 - view.width) // 2, y + 65 + (780 - view.height) // 2))
    contact.save(args.output / 'overview.png')
    def data_uri(path):
        mime = 'image/gif' if path.suffix == '.gif' else 'image/png'
        return 'data:' + mime + ';base64,' + base64.b64encode(path.read_bytes()).decode()
    cards = ''.join('<article><h2>' + html.escape(title) + '</h2><p>' + html.escape(detail) + '</p><a href="#" class="open" aria-label="Bild vergrößern"><img alt="' + html.escape(title) + '" src="' + data_uri(args.output / (key + '.png')) + '"></a></article>' for key, title, detail, _ in originals)
    page = '''<!doctype html><html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Practice Suite 7.1.0 / mab — Galerie</title><style>
:root{color-scheme:dark}*{box-sizing:border-box}body{margin:0;background:#090c0f;color:#e8f2f2;font:17px/1.55 system-ui,sans-serif}main{max-width:1420px;margin:auto;padding:42px 24px}h1{font-weight:550;letter-spacing:.04em}header small{color:#5ed4c4}p{color:#a0b8bc}header p{max-width:1000px}.tour{max-width:1150px;margin:30px auto}img{display:block;max-width:100%;height:auto;border-radius:9px}section{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,480px),1fr));gap:22px}article{padding:20px;border:1px solid #30454d;border-radius:14px;background:#0d1418}h2{margin:0;font-size:22px;color:#5ed4c4}article p{min-height:52px}.open{display:block;cursor:zoom-in}dialog{border:1px solid #38585e;padding:44px 12px 12px;max-width:98vw;max-height:98vh;background:#0b1013;border-radius:12px}dialog::backdrop{background:#000d}dialog img{max-height:86vh;max-width:94vw;width:auto;margin:auto}button{position:absolute;right:12px;top:9px;background:#1b353a;color:white;border:1px solid #5ed4c4;border-radius:6px;padding:4px 12px;cursor:pointer}a:focus-visible,button:focus-visible{outline:3px solid #5ed4c4;outline-offset:4px}@media(prefers-reduced-motion:reduce){*{scroll-behavior:auto}}
</style><main><header><small>IT STEALS / PRACTICE TOOLS</small><h1>PRACTICE SUITE <small>7.1.0 / mab</small></h1><p>Die tatsächlich implementierte Oberfläche. Die Bilder stammen aus der nativen ImGui-Zeichenausgabe mit synthetischen Testdaten, nicht aus einem Windows-/Unity-Livetest. Zum Vergrößern auf ein Bild klicken. Die Animation hat keinen Ton.</p></header><figure class="tour"><img alt="Animierter Rundgang durch die native Oberfläche" src="''' + data_uri(gif) + '''"></figure><section>''' + cards + '''</section><p>Die HUD-Kachel kombiniert getrennte Profilvorschauen für bessere Lesbarkeit. Im Spiel bleibt die Minimap im Clean-Speedrun gesperrt. Hide & Seek und Outside bleiben für Minimap und Seed-Suche WIP. Vorhandene Farb-, Audio- und Animationseinstellungen bleiben wirksam.</p></main><dialog><button type="button" aria-label="Bild schließen">Schließen [Esc]</button><img alt="Vergrößerte native Oberfläche"></dialog><script>
const box=document.querySelector('dialog'),large=box.querySelector('img');document.querySelectorAll('.open').forEach(a=>a.addEventListener('click',e=>{e.preventDefault();large.src=a.querySelector('img').src;large.alt=a.querySelector('img').alt;box.showModal()}));box.querySelector('button').onclick=()=>box.close();box.addEventListener('click',e=>{if(e.target===box)box.close()});
</script></html>'''
    (args.output / 'gallery.html').write_text(page, encoding='utf-8')
    (args.output / 'README.md').write_text('# Native GUI-Vorschau 7.1.0 / mab\n\nEchte ImGui-Zeichenausgabe mit synthetischen Testdaten; keine Windows-/Unity-Liveaufnahme. gallery.html ist selbstständig und bietet vergrößerbare Einzelbilder. interface-tour.gif zeigt die vorhandenen Effekte ohne Ton. Der eigene PB-Ton ist im Trainer hörbar. Die HUD-Kachel kombiniert voneinander unabhängige Profilvorschauen; die Minimap bleibt im Clean-Speedrun gesperrt.\n\nFont-Rasterung im Host: DejaVu Sans als Testersatz. Windows verwendet die im Trainer gewählte verfügbare Schrift. Bilder werden ausschließlich aus tests/ui_tests.cpp und design_ui_cases.inl erzeugt; keine erfundene Spielaufnahme.\n', encoding='utf-8')
    exports = {'interface-tour.gif': 'PracticeSuite_710_Tour.gif', 'overview.png': 'PracticeSuite_710_Overview.png', 'gallery.html': 'PracticeSuite_710_Gallery.html'}
    for name, dest in exports.items():
        (args.exports / dest).write_bytes((args.output / name).read_bytes())
    print(json.dumps({'chapters': len(chapters), 'frames': len(frames), 'duration_ms': sum(durations), 'exports': exports}), flush=True)


if __name__ == '__main__':
    main()
