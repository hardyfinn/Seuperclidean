#!/usr/bin/env python3
# Draws res/Seuperclidean4.svg (the 4-voice module). VCV Rack can't draw SVG <text>, so every label
# is turned into outline paths with fontTools. Positions are in mm and must
# match src/Seuperclidean4.cpp.
#   pip install fonttools
#   python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 28 * 5.08, 128.5  # 28 HP

BG = "#ECE6DA"
INK = "#1E1E24"
SOFT = "#7A7468"
ACCENT = "#C8502D"
OUTBOX = "#2A2A31"
OUTINK = "#ECE6DA"
RULE = "#CFC7B8"


fonts = {}


def text(s, x, y, size, font=FONT_MED, fill=INK, anchor="middle", spacing=0.04):
    """Return an SVG path for string s; (x, y) is the baseline point in mm."""
    if font not in fonts:
        f = TTFont(font)
        fonts[font] = (f, f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm)
    f, gs, cmap, upm = fonts[font]
    scale = size / upm
    track = spacing * upm
    names = [cmap[ord(c)] for c in s]
    width = sum(gs[n].width + track for n in names) - track
    if anchor == "middle":
        x0 = x - width * scale / 2
    elif anchor == "end":
        x0 = x - width * scale
    else:
        x0 = x
    pen = SVGPathPen(gs)
    cx = 0.0
    for n in names:
        tp = TransformPen(pen, (scale, 0, 0, -scale, x0 + cx * scale, y))
        gs[n].draw(tp)
        cx += gs[n].width + track
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'


# layout, same numbers as src/Seuperclidean4.cpp
COL_X = [19.5, 54.0, 88.5, 123.0]
RING_Y, RING_R = 26.0, 9.5
Y_KNOB1, Y_KNOB2, Y_TRIM, Y_CV, Y_OUT = 46.0, 58.5, 70.5, 82.0, 94.6
Y_BOTTOM = 111.6
LOGIC_X = [80.0, 96.0, 112.0, 128.0]
PAIRS = ["1+2", "3+4", "1+3", "2+4"]
VOICE_COL = ["#E0692E", "#C9A21E", "#2BAFB9", "#9670D8"]

parts = []
add = parts.append
add(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')

add(text("SEUPERCLIDEAN 4", W / 2, 8.6, 4.0, FONT_BOLD, INK, spacing=0.08))
add(text("four biased euclidean voices", W / 2, 12.2, 2.6, FONT_MED, SOFT))
add(f'<rect x="4" y="13.4" width="{W - 8:.2f}" height="0.35" fill="{ACCENT}"/>')

def lab(s, x, y, size=1.7, color=INK, font=FONT_BOLD):
    add(text(s, x, y, size, font, color))

for i, x in enumerate(COL_X):
    # ring background is drawn by the module; a thin colour bar marks the voice
    add(f'<rect x="{x - 6:.2f}" y="38.0" width="12" height="0.6" rx="0.3" fill="{VOICE_COL[i]}"/>')
    lab("LEN", x - 7.0, Y_KNOB1 - 4.6, 3.2); lab("FILL", x + 7.0, Y_KNOB1 - 4.6, 3.2)
    lab("ROT", x - 7.0, Y_KNOB2 - 4.6, 3.2); lab("BIAS", x + 7.0, Y_KNOB2 - 4.6, 3.2)
    lab("SWG", x - 10.0, Y_TRIM - 4.2, 2.9); lab("HUM", x, Y_TRIM - 4.2, 2.9); lab("MUT", x + 10.0, Y_TRIM - 4.2, 2.9)
    lab("CV  FILL \u00b7 ROT \u00b7 BIAS", x, Y_CV - 5.1, 2.2, SOFT, FONT_MED)
    add(f'<rect x="{x - 16:.2f}" y="{Y_OUT - 7.6:.2f}" width="32" height="12.6" rx="1.2" fill="{OUTBOX}"/>')
    for dx, s in [(-10.5, "GATE"), (0, "OFF"), (10.5, "ACC")]:
        lab(s, x + dx, Y_OUT - 4.9, 2.9, OUTINK)
    if i < 3:
        add(f'<rect x="{(x + COL_X[i + 1]) / 2 - 0.1:.2f}" y="40" width="0.2" height="45" fill="{RULE}"/>')

add(f'<rect x="4" y="102.5" width="{W - 8:.2f}" height="0.35" fill="{ACCENT}"/>')
for x, s in [(9, "CLOCK"), (21, "RESET"), (33, "FILL"), (44, "DENS"), (54, "MUT"), (64, "LOCK")]:
    lab(s, x, Y_BOTTOM - 4.9, 2.9)
add(text("thin \u2013 thick", 44, Y_BOTTOM + 6.8, 2.0, FONT_MED, SOFT))
add(f'<rect x="70" y="104.2" width="{W - 74:.2f}" height="19.5" rx="1.2" fill="{OUTBOX}"/>')
for x, p in zip(LOGIC_X, PAIRS):
    lab(p, x, 107.6, 2.9, OUTINK)
add(text("click a mode to change it", (70 + W - 4) / 2, 126.9, 2.0, FONT_MED, SOFT))

svg = (
    f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" '
    f'viewBox="0 0 {W:.2f} {H}">\n' + "\n".join(parts) + "\n</svg>\n"
)
with open(os.path.join(HERE, "Seuperclidean4.svg"), "w") as fh:
    fh.write(svg)
print("wrote Seuperclidean4.svg")

# --- Haer maker logo (res/haer_logo.py), placed clear of the jacks and rail artwork
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
import haer_logo
haer_logo.add_to_file(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "Seuperclidean4.svg"), x=71.14, y=126.3, cap=3.0, bg='#ECE6DA')
