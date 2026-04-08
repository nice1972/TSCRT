#!/usr/bin/env python3
"""
Generate tscrt_win icon (.ico) at multiple resolutions plus a PNG used
for in-app branding. Run once whenever the icon design changes:

    python resources/make_icon.py
"""
from PIL import Image, ImageDraw, ImageFont
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SIZES = (16, 24, 32, 48, 64, 128, 256)


def render(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d   = ImageDraw.Draw(img)
    pad = max(1, size // 16)

    # Rounded background tile
    bg_top    = (10, 30, 60, 255)
    bg_bottom = (30, 90, 160, 255)
    for y in range(size):
        t = y / max(1, size - 1)
        col = tuple(int(bg_top[i] * (1 - t) + bg_bottom[i] * t) for i in range(4))
        d.line([(0, y), (size, y)], fill=col)

    # Border
    d.rounded_rectangle(
        [(pad, pad), (size - pad, size - pad)],
        radius=max(2, size // 8),
        outline=(180, 220, 255, 255),
        width=max(1, size // 24),
    )

    # Stylized "T>" prompt-like glyph
    try:
        font = ImageFont.truetype("consola.ttf", int(size * 0.55))
    except OSError:
        font = ImageFont.load_default()
    text = "T>"
    bbox = d.textbbox((0, 0), text, font=font)
    tw   = bbox[2] - bbox[0]
    th   = bbox[3] - bbox[1]
    tx   = (size - tw) // 2 - bbox[0]
    ty   = (size - th) // 2 - bbox[1]
    # Drop shadow
    d.text((tx + max(1, size // 32), ty + max(1, size // 32)),
           text, font=font, fill=(0, 0, 0, 200))
    d.text((tx, ty), text, font=font, fill=(220, 240, 255, 255))
    return img


def main() -> None:
    images = [render(s) for s in SIZES]
    ico_path = ROOT / "tscrt_win.ico"
    images[0].save(ico_path, format="ICO",
                   sizes=[(s, s) for s in SIZES],
                   append_images=images[1:])
    print(f"wrote {ico_path}")

    png_path = ROOT / "tscrt_win.png"
    render(256).save(png_path, format="PNG")
    print(f"wrote {png_path}")


if __name__ == "__main__":
    main()
