"""Generates the MagnifyFactory app icon (resources/icon.ico).

Design: a magnifying glass (the "Magnify" in the name) over a stack of
layered bars (the "Factory" — a conveyor of files being processed), in the
app's own dark/blue palette. Rendered at multiple sizes into one .ico.
"""
from PIL import Image, ImageDraw

SIZE = 512
BG = (30, 33, 38, 255)         # #1e2126 — app background
ACCENT = (47, 111, 235, 255)   # #2f6feb — primary accent
ACCENT_LIGHT = (88, 166, 255, 255)  # #58a6ff
PANEL = (45, 51, 59, 255)      # #2d333b


def draw_rounded_bg(draw, size, radius, color):
    draw.rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=color)


def build_icon() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    draw_rounded_bg(draw, SIZE, radius=100, color=BG)

    # "Factory" stack: three layered bars, bottom-left, representing files
    # queued/processed.
    bar_color_steps = [PANEL, (58, 64, 72, 255), ACCENT]
    bar_w, bar_h = 210, 46
    x0, y0 = 70, 300
    for i, color in enumerate(bar_color_steps):
        y = y0 + i * (bar_h + 14)
        draw.rounded_rectangle([x0 + i * 14, y, x0 + i * 14 + bar_w, y + bar_h], radius=10, fill=color)

    # Magnifying glass: ring + handle, large, overlapping the stack.
    cx, cy, r = 300, 210, 118
    ring_width = 34
    draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=ACCENT_LIGHT, width=ring_width)
    # subtle glass fill
    inner_r = r - ring_width // 2 - 6
    draw.ellipse([cx - inner_r, cy - inner_r, cx + inner_r, cy + inner_r], fill=(88, 166, 255, 40))

    # Handle
    import math
    angle = math.radians(45)
    hx0 = cx + (r - 6) * math.cos(angle)
    hy0 = cy + (r - 6) * math.sin(angle)
    hx1 = hx0 + 150 * math.cos(angle)
    hy1 = hy0 + 150 * math.sin(angle)
    draw.line([hx0, hy0, hx1, hy1], fill=ACCENT_LIGHT, width=46)
    draw.ellipse([hx1 - 23, hy1 - 23, hx1 + 23, hy1 + 23], fill=ACCENT_LIGHT)

    return img


if __name__ == "__main__":
    icon = build_icon()
    icon.save("icon.png")
    sizes = [16, 24, 32, 48, 64, 128, 256]
    icon.save("icon.ico", sizes=[(s, s) for s in sizes])
    print("Wrote icon.png and icon.ico")
