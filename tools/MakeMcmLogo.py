"""Cut the title lettering out of the banner onto a transparent background.

Usage (requires Pillow):
    python tools/MakeMcmLogo.py <banner image> <output folder>

Writes logo.dds (copy to dist/Data/Interface/ArtisansOfSkyrim/mcm_logo.dds)
and logo_preview.png (copy to interface/mcm/mcm_logo.png). The crop box and
clean-up areas below match the 1672x941 Nexus banner.
"""

import sys
from pathlib import Path

from PIL import Image, ImageFilter

source, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
image = Image.open(source).convert("RGB")
width, height = image.size

# Title area only (sword, gem and potion stay outside).
box = (395, 250, 1345, 650)
crop = image.crop(box)

pixels = crop.load()
alpha = Image.new("L", crop.size, 0)
mask = alpha.load()
for y in range(crop.size[1]):
    for x in range(crop.size[0]):
        r, g, b = pixels[x, y]
        luminance = 0.299 * r + 0.587 * g + 0.114 * b
        saturation = max(r, g, b) - min(r, g, b)
        # Silver letters are grey (R, G and B close) and fairly bright, even
        # where the soul gem tints them. Wood (brown) and the gem's glow
        # (purple) are strongly coloured, so they fade out.
        score = (luminance - 38) / (95 - 38)
        if saturation > 16:
            score -= (saturation - 16) / 10
        # Tip of the sword guard, left of the title.
        if x < 30 and y > 190:
            score = 0.0
        mask[x, y] = int(max(0.0, min(1.0, score)) * 255)

# Drop isolated grain specks without thinning the strokes.
alpha = alpha.filter(ImageFilter.MedianFilter(3))


def remove_small_blobs(mask_image, minimum_area=60, threshold=40):
    """Clear connected spots smaller than any letter (glints, wood grain)."""
    w, h = mask_image.size
    data = mask_image.load()
    seen = bytearray(w * h)
    for start_y in range(h):
        for start_x in range(w):
            index = start_y * w + start_x
            if seen[index] or data[start_x, start_y] < threshold:
                continue
            blob, stack = [], [(start_x, start_y)]
            seen[index] = 1
            while stack:
                x, y = stack.pop()
                blob.append((x, y))
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < w and 0 <= ny < h:
                        n = ny * w + nx
                        if not seen[n] and data[nx, ny] >= threshold:
                            seen[n] = 1
                            stack.append((nx, ny))
            if len(blob) < minimum_area:
                for x, y in blob:
                    data[x, y] = 0
    return mask_image


alpha = remove_small_blobs(alpha)

# The engraved shadows cannot be separated from dark wood, so rebuild depth
# with a soft dark shadow behind the letters.
shadow = alpha.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(2.5))
letters = crop.convert("RGBA")
letters.putalpha(alpha)
logo = Image.new("RGBA", crop.size, (0, 0, 0, 0))
logo.putalpha(shadow.point(lambda value: int(value * 0.85)))
logo.alpha_composite(letters)
logo = logo.crop(logo.getbbox())

# Power-of-two canvas for the game's texture loader.
CANVAS = (512, 256)
canvas = Image.new("RGBA", CANVAS, (0, 0, 0, 0))
scale = min(500 / logo.width, 244 / logo.height, 1.0)
if scale < 1.0:
    logo = logo.resize((round(logo.width * scale), round(logo.height * scale)), Image.LANCZOS)
canvas.alpha_composite(logo, ((CANVAS[0] - logo.width) // 2, (CANVAS[1] - logo.height) // 2))

out_dir.mkdir(parents=True, exist_ok=True)
canvas.save(out_dir / "logo_preview.png")
canvas.save(out_dir / "logo.dds")
preview = Image.new("RGBA", canvas.size, (40, 40, 40, 255))
preview.alpha_composite(canvas)
preview.convert("RGB").save(out_dir / "logo_on_grey.png")
print("logo size", logo.size)
