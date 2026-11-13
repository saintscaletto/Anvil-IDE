#!/usr/bin/env python3
"""Generate the Anvil application icon (PNG) without external dependencies."""
from pathlib import Path
from PIL import Image, ImageDraw

SCALE = 4
SIZE = 512
W = SIZE * SCALE
img = Image.new("RGBA", (W, W), (0, 0, 0, 0))
draw = ImageDraw.Draw(img)


def s(value: float) -> int:
    return int(round(value * SCALE))


# Rounded, dark app tile.
draw.rounded_rectangle((s(8), s(8), s(504), s(504)), radius=s(96), fill="#1e1f22",
                       outline="#393b40", width=s(6))
draw.rounded_rectangle((s(20), s(20), s(492), s(492)), radius=s(84), outline="#2e436e",
                       width=s(2))

# Anvil body.
accent = "#4a88c7"
accent_light = "#7db4e6"
accent_dark = "#35618f"
steel_light = "#a9cdf0"

# Base.
draw.rounded_rectangle((s(154), s(366), s(358), s(410)), radius=s(16), fill=accent_dark)
# Waist / body.
draw.polygon([(s(212), s(300)), (s(300), s(300)), (s(282), s(368)), (s(230), s(368))],
             fill=accent)
# Top face.
draw.polygon(
    [
        (s(104), s(236)),
        (s(292), s(236)),
        (s(330), s(200)),
        (s(424), s(200)),
        (s(446), s(238)),
        (s(386), s(266)),
        (s(330), s(276)),
        (s(318), s(304)),
        (s(168), s(304)),
        (s(154), s(276)),
        (s(116), s(258)),
    ],
    fill=accent,
)
# Top highlight.
draw.polygon(
    [
        (s(104), s(236)),
        (s(292), s(236)),
        (s(330), s(200)),
        (s(424), s(200)),
        (s(430), s(208)),
        (s(330), s(218)),
        (s(298), s(246)),
        (s(112), s(246)),
    ],
    fill=steel_light,
)
# Face edge.
draw.line([(s(168), s(304)), (s(318), s(304))], fill=accent_light, width=s(5))
draw.line([(s(330), s(276)), (s(386), s(266))], fill=accent_light, width=s(5))

# Forge spark.
spark = "#e5b567"
draw.polygon(
    [
        (s(372), s(112)),
        (s(384), s(146)),
        (s(418), s(158)),
        (s(384), s(170)),
        (s(372), s(204)),
        (s(360), s(170)),
        (s(326), s(158)),
        (s(360), s(146)),
    ],
    fill=spark,
)
draw.ellipse((s(410), s(206), s(424), s(220)), fill=spark)
draw.ellipse((s(332), s(188), s(342), s(198)), fill=spark)

img = img.resize((SIZE, SIZE), Image.LANCZOS)
root = Path(__file__).resolve().parent.parent
img.save(root / "assets" / "anvil-512.png")
img.resize((256, 256), Image.LANCZOS).save(root / "assets" / "anvil-256.png")
print("wrote assets/anvil-512.png and assets/anvil-256.png")
