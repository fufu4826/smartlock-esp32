from pathlib import Path
from gen_thai_reset_bitmaps import bitmap
labels=['กรุณาใส่รหัส','ลบ','ตกลง','ยกเลิก','ปลดล็อกฉุกเฉิน','รีเซ็ตโรงงาน','กลับ','ต้องการปลดล็อกใช่หรือไม่','ปลดล็อก','ข้อมูลทั้งหมดจะถูกลบ','ดำเนินการต่อ','ยืนยันรีเซ็ตโรงงาน','กดค้างเพื่อรีเซ็ต','รหัสไม่ถูกต้อง','รอ 60 วินาที','ระบบไม่พร้อม']
labels.extend([
 ('\u0e15\u0e31\u0e49\u0e07\u0e04\u0e48\u0e32 LINE',22),
 ('\u0e15\u0e31\u0e49\u0e07\u0e04\u0e48\u0e32 LINE \u0e1c\u0e48\u0e32\u0e19 USB',22),
 ('\u0e40\u0e0a\u0e37\u0e48\u0e2d\u0e21\u0e15\u0e48\u0e2d USB \u0e40\u0e1e\u0e37\u0e48\u0e2d\u0e15\u0e31\u0e49\u0e07\u0e04\u0e48\u0e32',20),
 ('\u0e08\u0e33\u0e01\u0e31\u0e14\u0e40\u0e27\u0e25\u0e32 120 \u0e27\u0e34\u0e19\u0e32\u0e17\u0e35',20),
])
out=['#pragma once','#include <Arduino.h>','struct AdminLabel {const uint8_t* data;uint16_t w,h;};']
for i,item in enumerate(labels):
 text,size=item if isinstance(item,tuple) else (item,22)
 w,h,data=bitmap(text,size)
 out.append('const uint8_t adminBits%d[] PROGMEM={%s};'%(i,','.join(str(b) for b in data)))
 out.append('const AdminLabel adminLabel%d={adminBits%d,%d,%d};'%(i,i,w,h))
Path('src/hardware/AdminBitmaps.h').write_text('\n'.join(out)+'\n',encoding='ascii')
