#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render the application icon in every format the packages need.

Reads packaging/icons/nmeasimulatorx.svg, the only source of the icon, rasterises it with
cairosvg and writes three files next to it with Pillow, overwriting them:

- `nmeasimulatorx.png`, 256 x 256 pixels: the window icon compiled into the application and
  the 256-pixel hicolor icon installed on Linux;
- `nmeasimulatorx.ico` with the sizes 16, 24, 32, 48, 64 and 256 pixels: the icon of the
  Windows executable and of the NSIS installer;
- `nmeasimulatorx.icns` with the sizes 32 to 1024 pixels: the icon of the macOS bundle.

Run it after changing the SVG and commit the outputs; the build uses the committed files and
does not run this script. It requires cairosvg and Pillow and takes no arguments; the paths
are relative to the script, so it runs from any directory:

    pip install cairosvg pillow
    python3 tools/render_icons.py

Exit status:
    0 on success. A missing package or an unreadable SVG ends the script with a traceback and
    status 1.
"""

import io
import pathlib

import cairosvg
from PIL import Image

#: Root of the repository.
ROOT = pathlib.Path(__file__).resolve().parent.parent
#: Directory of the icon source and of the rendered files.
ICONS = ROOT / "packaging" / "icons"
#: The SVG every rendered file is made from.
SOURCE = ICONS / "nmeasimulatorx.svg"


def render(size: int) -> Image.Image:
    """Rasterise the SVG source at a square size.

    Args:
        size: Width and height of the image in pixels.

    Returns:
        The rendered image in RGBA mode, keeping the transparency of the SVG.
    """
    png = cairosvg.svg2png(url=str(SOURCE), output_width=size, output_height=size)
    return Image.open(io.BytesIO(png)).convert("RGBA")


def main() -> None:
    """Render the PNG, ICO and ICNS files from the SVG source.

    Raises:
        OSError: The SVG cannot be read or an output file cannot be written.
    """
    render(256).save(ICONS / "nmeasimulatorx.png")
    # Pillow scales the 256-pixel image down to each smaller size of the ICO file.
    render(256).save(
        ICONS / "nmeasimulatorx.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (256, 256)]
    )
    # Pillow scales this image to every size of the ICNS file, the largest of which is 1024.
    render(1024).save(ICONS / "nmeasimulatorx.icns")


if __name__ == "__main__":
    main()
