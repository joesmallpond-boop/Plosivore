# Renders the installer sidebar background (bottom-left anchored, clear of the step list).
# Run from this folder: python3 make_background.py
import math
from PIL import Image, ImageDraw, ImageFont
W, H, S = 620, 418, 2
pop = (255, 79, 154)
# Light Installer draws dark step-list text, dark mode draws light text: one image for each.
VARIANTS = {'background': ((246, 240, 250), (36, 24, 47)),        # pale lilac bg, plum wordmark
            'background-dark': ((36, 24, 47), (236, 223, 245))}   # plum bg, lilac wordmark

def render(name, bg, ink):
  img = Image.new('RGB', (W * S, H * S), bg); d = ImageDraw.Draw(img)
  font = ImageFont.truetype('../../Resources/Bricolage-ExtraBold.ttf', 30 * S)
  dot = lambda x, y, r, c: d.ellipse([(x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S], fill=c)
  for x, y, r in [(30, 330, 3), (52, 312, 4), (76, 300, 3.5), (100, 292, 4.5)]:  # caught pops drifting off
      dot(x, y, r, pop)
  cx, cy, R = 136, 300, 12                                                        # one mid-burst
  d.ellipse([(cx - R) * S, (cy - R) * S, (cx + R) * S, (cy + R) * S], outline=pop, width=3 * S)
  for k in range(7):
      a = k / 7 * 2 * math.pi + 0.4
      dot(cx + math.cos(a) * (R + 8), cy + math.sin(a) * (R + 8), 2.2, pop)
  d.text((24 * S, (H - 64) * S), "plosivore", font=font, fill=ink)
  img.save(f'resources/{name}@2x.png')
  img.resize((W, H), Image.LANCZOS).save(f'resources/{name}.png')

for n, (bg, ink) in VARIANTS.items():
    render(n, bg, ink)
