from pathlib import Path
from gen_thai_reset_bitmaps import bitmap
labels = [
 ('FallbackButton','\u0e40\u0e27\u0e47\u0e1a\u0e44\u0e21\u0e48\u0e40\u0e1b\u0e34\u0e14? \u0e40\u0e0a\u0e37\u0e48\u0e2d\u0e21\u0e15\u0e48\u0e2d SmartLock',18),
 ('FallbackTitle','\u0e40\u0e0a\u0e37\u0e48\u0e2d\u0e21\u0e15\u0e48\u0e2d Wi-Fi SmartLock',22),
 ('FallbackScan','\u0e2a\u0e41\u0e01\u0e19\u0e41\u0e25\u0e49\u0e27\u0e40\u0e25\u0e37\u0e2d\u0e01\u0e40\u0e0a\u0e37\u0e48\u0e2d\u0e21\u0e15\u0e48\u0e2d',20),
 ('FallbackStay','\u0e40\u0e25\u0e37\u0e2d\u0e01\u0e43\u0e0a\u0e49 Wi-Fi \u0e19\u0e35\u0e49\u0e41\u0e21\u0e49\u0e44\u0e21\u0e48\u0e21\u0e35\u0e2d\u0e34\u0e19\u0e40\u0e17\u0e2d\u0e23\u0e4c\u0e40\u0e19\u0e47\u0e15',17),
 ('FallbackBack','\u0e41\u0e15\u0e30\u0e40\u0e1e\u0e37\u0e48\u0e2d\u0e01\u0e25\u0e31\u0e1a\u0e44\u0e1b\u0e2a\u0e41\u0e01\u0e19 QR',20),
]
out=['#pragma once','#include <Arduino.h>']
for name,text,size in labels:
 w,h,data=bitmap(text,size)
 assert w<=300
 out += [f'constexpr uint16_t k{name}Width={w}, k{name}Height={h};',f'const uint8_t k{name}[] PROGMEM={{{",".join(map(str,data))}}};']
Path('src/hardware/LocalFallbackBitmaps.h').write_text('\n'.join(out)+'\n',encoding='ascii')
