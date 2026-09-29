#!/usr/bin/env python3
"""Generate compact 1-bit Thai labels for the reset screens."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "src" / "hardware" / "ThaiResetBitmaps.h"
FONT = Path(r"C:\Windows\Fonts\tahoma.ttf")
ASSETS = {
    "kThaiResetQuestionLine1": ("คุณต้องการ", 30),
    "kThaiResetQuestionLine2": ("รีเซ็ตใช่ไหม", 30),
    "kThaiResetConfirmLine1": ("ยืนยันการรีเซ็ต", 28),
    "kThaiResetConfirmLine2": ("คืนค่าโรงงานทั้งหมด", 26),
    "kThaiResetting": ("กำลังรีเซ็ต", 30),
    "kThaiYes": ("ใช่", 28),
    "kThaiNo": ("ไม่", 28),
    "kThaiOkay": ("โอเค", 28),
}


def bitmap(text: str, size: int) -> tuple[int, int, bytes]:
    font = ImageFont.truetype(str(FONT), size)
    scratch = Image.new("L", (1, 1), 0)
    bounds = ImageDraw.Draw(scratch).textbbox((0, 0), text, font=font, stroke_width=0)
    width, height = bounds[2] - bounds[0] + 2, bounds[3] - bounds[1] + 2
    image = Image.new("L", (width, height), 0)
    ImageDraw.Draw(image).text((1 - bounds[0], 1 - bounds[1]), text, font=font, fill=255)
    stride = (width + 7) // 8
    packed = bytearray(stride * height)
    for y in range(height):
        for x in range(width):
            if image.getpixel((x, y)) >= 128:
                packed[y * stride + x // 8] |= 0x80 >> (x % 8)
    return width, height, bytes(packed)


def main() -> None:
    lines = ["#pragma once", "", "#include <Arduino.h>", ""]
    for name, (text, size) in ASSETS.items():
        width, height, data = bitmap(text, size)
        lines.append(f"constexpr uint16_t {name}Width = {width};")
        lines.append(f"constexpr uint16_t {name}Height = {height};")
        lines.append(f"const uint8_t {name}[] PROGMEM = {{")
        for start in range(0, len(data), 12):
            chunk = ", ".join(f"0x{byte:02X}" for byte in data[start : start + 12])
            lines.append(f"  {chunk},")
        lines.extend(["};", ""])
    OUT.write_text("\n".join(lines), encoding="ascii")
    print(f"Wrote {OUT}")


if __name__ == "__main__":
    main()
