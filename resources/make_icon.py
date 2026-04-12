#!/usr/bin/env python3
"""
Generate the TSCRT app icon assets at multiple resolutions:
  - tscrt_win.ico  (Windows multi-resolution icon)
  - tscrt_mac.icns (macOS bundle icon)
  - tscrt_win.png  (256x256 PNG used for in-app branding)

Run once whenever the icon design changes:

    python resources/make_icon.py
"""
from PIL import Image, ImageDraw, ImageFont
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SIZES = (16, 24, 32, 48, 64, 128, 256)
ICNS_SIZES = (16, 32, 64, 128, 256, 512)


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

    icns_path = ROOT / "tscrt_mac.icns"
    icns_images = [render(s) for s in ICNS_SIZES]
    icns_images[-1].save(icns_path, format="ICNS",
                         append_images=icns_images[:-1],
                         sizes=[(s, s) for s in ICNS_SIZES])
    print(f"wrote {icns_path}")


if __name__ == "__main__":
    main()
