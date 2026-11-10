#!/usr/bin/env python3
# Draws res/Seuperclidean.svg. VCV Rack can't draw SVG <text>, so every label
# is turned into outline paths with fontTools. Positions are in mm and must
# match src/Seuperclidean.cpp.
#   pip install fonttools
#   python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 38 * 5.08, 128.5  # 38 HP

BG = "#ECE6DA"
INK = "#1E1E24"
SOFT = "#7A7468"
ACCENT = "#C8502D"
OUTBOX = "#2A2A31"
OUTINK = "#ECE6DA"
RULE = "#CFC7B8"

# layout, same numbers as the C++
ROW_Y0, ROW_DY = 48.0, 12.5
X_LEN, X_FILL, X_ROT, X_BIAS = 12.0, 23.0, 34.0, 45.0
X_SWG, X_HUM, X_MUT = 55.5, 65.0, 74.5
X_CFILL, X_CROT, X_CBIAS = 85.0, 95.0, 105.0
X_GATE, X_OFF, X_ACC = 116.0, 126.0, 136.0
RA, RB, RC = 155.0, 169.0, 183.0
LOGIC_Y = [84.6, 98.6, 112.6]

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


parts = []
add = parts.append
add(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')

# Title over the display
add(text("SEUPERCLIDEAN", 6.0, 9.6, 4.6, FONT_BOLD, INK, anchor="start", spacing=0.08))
add(text("biased euclidean rhythms that mutate", 141.0, 9.6, 2.8, FONT_MED, SOFT, anchor="end"))
add(f'<rect x="5" y="11.2" width="137" height="0.35" fill="{ACCENT}"/>')

# Channel area headers
hy = 41.4
groups = [
    (X_LEN, "LEN"), (X_FILL, "FILL"), (X_ROT, "ROT"), (X_BIAS, "BIAS"),
    (X_SWG, "SWG"), (X_HUM, "HUM"), (X_MUT, "MUT"),
    (X_CFILL, "FILL"), (X_CROT, "ROT"), (X_CBIAS, "BIAS"),
]
for x, label in groups:
    add(text(label, x, hy, 3.0, FONT_BOLD))
add(text("CV", (X_CFILL + X_CBIAS) / 2, hy - 4.0, 2.4, FONT_MED, SOFT))
add(text("early  -  late", X_BIAS, hy - 4.0, 2.2, FONT_MED, SOFT))

# dark box behind the outputs
add(f'<rect x="110.5" y="37.0" width="31.0" height="{ROW_Y0 + 5 * ROW_DY + 6.0 - 37.0:.2f}" rx="1.2" fill="{OUTBOX}"/>')
for x, label in [(X_GATE, "GATE"), (X_OFF, "OFF"), (X_ACC, "ACC")]:
    add(text(label, x, hy, 2.9, FONT_BOLD, OUTINK))

for i in range(6):
    y = ROW_Y0 + i * ROW_DY
    add(text(str(i + 1), 4.4, y + 1.2, 3.2, FONT_BOLD, ACCENT))
    if i < 5:
        add(f'<rect x="2.5" y="{y + 6.1:.2f}" width="107.0" height="0.2" fill="{RULE}"/>')

# vertical rule between the channel area and the right column
add(f'<rect x="145.0" y="12" width="0.25" height="{H - 18}" fill="{RULE}"/>')

# Right column
def lab(x, y, s, color=INK, size=2.9):
    add(text(s, x, y, size, FONT_BOLD, color))

lab(RA, 14.2, "CLOCK"); lab(RB, 14.2, "RESET"); lab(RC, 14.2, "FILL")
lab(RA, 28.0, "DENSITY", INK, 3.2); lab(RB, 28.6, "CV"); lab(RC, 28.6, "FILL")
lab(RA, 43.2, "MUTATE", INK, 3.2); lab(RB, 43.8, "CV"); lab(RC, 43.8, "AMT")
lab(RA, 58.2, "LOCK"); lab(RB, 58.2, "LOCK"); lab(RC, 58.2, "HOME")
add(text("gate", RB, 71.0, 2.2, FONT_MED, SOFT))

# Logic section
add(f'<rect x="147.0" y="72.8" width="{W - 150.0:.2f}" height="0.35" fill="{ACCENT}"/>')
lab(RA + 1.0, 77.0, "LOGIC", INK, 2.9)
add(f'<rect x="{RC + 2.0 - 5.5:.2f}" y="75.2" width="11.0" height="42.0" rx="1.2" fill="{OUTBOX}"/>')
add(text("OUT", RC + 2.0, 78.4, 2.6, FONT_MED, "#B8B0A2"))
modes = ["AND", "OR", "XOR", "A NOT B"]
for i, y in enumerate(LOGIC_Y):
    pair = ["1 + 2", "3 + 4", "5 + 6"][i]
    add(text(pair, RA - 1.0, y - 2.8, 2.9, FONT_BOLD, ACCENT))
    for m, name in enumerate(modes):
        add(text(name, RA + 8.6, y - 4.4 + m * 2.9 + 0.8, 2.2, FONT_MED, INK, anchor="start"))

add(text("after Toussaint's Euclidean rhythms", 72.0, 124.8, 2.4, FONT_MED, SOFT))

svg = (
    f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" '
    f'viewBox="0 0 {W:.2f} {H}">\n' + "\n".join(parts) + "\n</svg>\n"
)
with open(os.path.join(HERE, "Seuperclidean.svg"), "w") as fh:
    fh.write(svg)
print("wrote Seuperclidean.svg")
