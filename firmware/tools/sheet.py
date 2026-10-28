"""Combine snapshots/*.ppm into one contact sheet (round screen mask, labels)."""
import glob
import os

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
files = sorted(f for f in glob.glob(os.path.join(HERE, "snapshots", "*.ppm")) if "setup_" not in f and "selftest" not in f)
size, pad, cols = 466, 24, 4
rows = (len(files) + cols - 1) // cols
sheet = Image.new("RGB", (cols * (size + pad) + pad, rows * (size + pad + 30) + pad), "#1b1c28")
mask = Image.new("L", (size, size), 0)
ImageDraw.Draw(mask).ellipse((0, 0, size - 1, size - 1), fill=255)
draw = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    x = pad + (i % cols) * (size + pad)
    y = pad + (i // cols) * (size + pad + 30)
    sheet.paste(Image.open(f), (x, y), mask)
    draw.ellipse((x, y, x + size - 1, y + size - 1), outline="#3b4070", width=3)
    draw.text((x + size // 2, y + size + 14), os.path.basename(f)[:-4].split("_", 1)[1], fill="#c9cdf0", anchor="mm",
              font_size=22)
out = os.path.join(HERE, "..", "docs", "faces.png")
sheet.save(out)
print("wrote", os.path.normpath(out))

# The self-test screen
test = os.path.join(HERE, "snapshots", "selftest.ppm")
if os.path.exists(test):
    img = Image.new("RGB", (size, size), "#1b1c28")
    img.paste(Image.open(test), (0, 0), mask)
    out = os.path.join(HERE, "..", "docs", "selftest.png")
    img.save(out)
    print("wrote", os.path.normpath(out))

# The setup screens side by side
setup = sorted(glob.glob(os.path.join(HERE, "snapshots", "setup_*.ppm")))
if setup:
    pair = Image.new("RGB", (len(setup) * (size + pad) - pad, size), "#1b1c28")
    for i, f in enumerate(setup):
        pair.paste(Image.open(f), (i * (size + pad), 0), mask)
    out = os.path.join(HERE, "..", "docs", "setup-screen.png")
    pair.save(out)
    print("wrote", os.path.normpath(out))
