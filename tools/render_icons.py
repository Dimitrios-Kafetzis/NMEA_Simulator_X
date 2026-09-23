#!/usr/bin/env python3
"""Render the application icon in every format the packages need.

The SVG under packaging/icons/ is the only source; run this script after changing it and
commit the outputs. Requires cairosvg and Pillow (pip install cairosvg pillow).
"""

import io
import pathlib

import cairosvg
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
ICONS = ROOT / "packaging" / "icons"
SOURCE = ICONS / "nmeasimulatorx.svg"


def render(size: int) -> Image.Image:
    png = cairosvg.svg2png(url=str(SOURCE), output_width=size, output_height=size)
    return Image.open(io.BytesIO(png)).convert("RGBA")


def main() -> None:
    render(256).save(ICONS / "nmeasimulatorx.png")
    render(256).save(
        ICONS / "nmeasimulatorx.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (256, 256)]
    )
    render(1024).save(ICONS / "nmeasimulatorx.icns")


if __name__ == "__main__":
    main()
