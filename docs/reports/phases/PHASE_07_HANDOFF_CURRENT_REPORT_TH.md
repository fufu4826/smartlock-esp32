# รายงานส่งต่อ GPT — Phase 7 และการปรับ UI/Factory Reset ล่าสุด

## 1. สถานะและขอบเขต

โครงการ: ESP32-035 SmartLock

- Workspace: `C:\ESP\esp32_035_lock_touch_test`
- บอร์ดที่เชื่อมต่อ: ESP32-035, COM6
- เฟิร์มแวร์: Arduino framework ผ่าน PlatformIO environment `esp32_035`
- AP ของบอร์ด: `SmartLock-F0A4`, IP `192.168.4.1`
- LAN IP ที่เคยทดสอบได้: `192.168.1.179` (อาจเปลี่ยนเมื่อ DHCP จัดสรรใหม่)
- ไม่มีการเพิ่ม Bluetooth
- Phase 6 การตรวจหลายอุปกรณ์ยังพักตามคำขอผู้ใช้
- Phase 6A เป็นการย้ายงาน LAN/STA จาก Phase 9 มาทำก่อนตามคำขอผู้ใช้
- **Phase 7 ยังไม่ประกาศ PASS และยังไม่เริ่มเฟสถัดไป** เพราะยังไม่มีผลยืนยันครบเส้นทาง LAN Owner → ปลดล็อกจริง → ล็อกกลับตามเวลา → ปฏิเสธ replay หลังการแก้ล่าสุด

รายงานนี้เป็นสถานะล่าสุดและใช้แทนคำอธิบาย UI เก่าในรายงานก่อนหน้า โดยเฉพาะ RESET QR, การแตะสามครั้ง และการยืนยันรีเซ็ตผ่านเว็บ ซึ่งไม่ได้เป็นเส้นทางใช้งานปัจจุบันแล้ว

## 2. ผลทดสอบ Phase 7 ที่ผู้ใช้เคยยืนยัน

| รายการ | ผลที่ผู้ใช้รายงาน |
|---|---|
| ESP32 เชื่อม Wi-Fi บ้าน | PASS |
| ACCESS QR เปิดหน้าเว็บผ่าน LAN IP | PASS |
| เบราว์เซอร์บน LAN รู้จัก Owner | FAIL ในการทดสอบครั้งแรก |
| ปลดล็อกแม่เหล็กผ่าน ACCESS | NOT REACHED ในการทดสอบครั้งแรก |
| แม่เหล็กยังล็อกและไม่มี unintended unlock | PASS ในการทดสอบครั้งแรก |

สาเหตุของ Owner recognition ที่ล้มเหลว: localStorage ของ `http://192.168.4.1` และ `http://192.168.1.179` เป็นคนละ origin แม้ฐานข้อมูล ESP32 จะเป็นชุดเดียวกัน

## 3. การแก้ Owner AP → LAN

ผู้ใช้ต้องการลงทะเบียน Owner ครั้งเดียว ไม่ต้องกรอกชื่อหรือกดลงทะเบียน Owner รอบสองเมื่อเปลี่ยน Wi-Fi

พฤติกรรมที่ทำแล้ว:

1. Owner ที่มี credential ใช้ MANAGEMENT QR เข้าหน้า Network บน AP
2. ตั้ง Wi-Fi บ้านผ่านหน้าเว็บ ไม่ hardcode SSID/password
3. ระหว่างเชื่อม STA ยังคง AP ไว้ด้วย AP+STA
4. เมื่อ STA Connected หน้าจัดการเตรียมลิงก์ส่งต่อสิทธิ์ให้อัตโนมัติ
5. ผู้ใช้สลับโทรศัพท์ไป Wi-Fi บ้านและเปิดลิงก์นั้นในเบราว์เซอร์เดิมภายในห้านาที
6. หน้า LAN ทำ handoff อัตโนมัติ ไม่มีช่องกรอกชื่อหรือปุ่มลงทะเบียนรอบสอง
7. หน้า LAN สร้าง opaque credential ใหม่ในเบราว์เซอร์ และบันทึกเข้า localStorage ของ LAN origin
8. ESP32 เก็บเฉพาะ salted verifier/hash และสร้าง Device record ที่ผูกกับ **Owner เดิม U000001** ไม่สร้าง Owner ซ้ำ

ข้อเท็จจริงทางเทคนิค: Owner account เป็นบัญชีเดิม แต่ credential ของ LAN origin เป็นอีก Device record เนื่องจาก browser storage แยก origin นี่ไม่ได้เป็นการนำ credential เดิมข้าม origin โดยตรง

ข้อกำหนดความปลอดภัยที่ยังคงอยู่:

- bootstrap token random 256 bits, อายุห้านาที, ใช้ครั้งเดียว
- สร้างได้จาก authenticated Owner Management เท่านั้น
- ผูกกับ Active Owner device และ LAN IP ปัจจุบัน
- permanent credential ไม่อยู่ใน URL หรือ Serial
- ใช้ server-side one-time bootstrap; ไม่ใช้ window.opener/postMessage
- consumed/expired/unknown bootstrap ถูกปฏิเสธ
- ไม่มีการปลดล็อกจาก route bootstrap

ข้อจำกัดที่ยังต้องตรวจ: หาก server รับ bootstrap สำเร็จแต่ response สูญหาย token จะถูกใช้แล้ว การลอง URL เดิมอาจถูกปฏิเสธและต้องสร้าง handoff link ใหม่ ไม่ควรอ้างว่า flow นี้ไม่มี failure case

## 4. สีและการควบคุมหน้าจอปัจจุบัน

| หน้า | สีพื้นหน้าจอ | การเปิด | QR |
|---|---|---|---|
| SETUP | น้ำเงิน, ตัวหนังสือขาว | แตะหนึ่งครั้งเมื่อยังไม่ตั้งค่า | ดำบนพื้นที่ขาว |
| ACCESS | ขาว | แตะหนึ่งครั้งเมื่อ configured | ดำบนพื้นที่ขาว |
| MANAGEMENT | เหลือง | กดค้างประมาณ 1.5 วินาที | ดำบนพื้นที่ขาว |
| RESET | แดง | ยกนิ้วแล้วกดค้างอีกครั้งใน MANAGEMENT | **ไม่มี QR** |

การแตะติดกันสามครั้งไม่ได้เปิด RESET อีกแล้ว

QR ที่ยังใช้ทุกหน้าจะมีพื้นที่ขาวรวม quiet zone เสมอ ไม่เปลี่ยน QR background เป็นสีน้ำเงินหรือเหลือง

## 5. Factory Reset บนจอจริง

เส้นทางล่าสุดตามคำขอผู้ใช้:

`ACCESS → hold → MANAGEMENT → release → hold → RESET`

หน้า RESET แสดงภาษาไทย “คุณต้องการรีเซ็ตใช่ไหม” พร้อมปุ่ม:

- **ไม่**: กลับ ACCESS โดยไม่ล้างข้อมูล
- **ใช่**: เปิดหน้ายืนยันแยกอีกหน้า มีปุ่ม **โอเค**
- แตะและยกนิ้วที่ **โอเค**: ล็อก GPIO22, ล้างข้อมูล, แล้วรีบูตกลับ initial setup

ปุ่มทำงานตอนยกนิ้วหลัง short tap จึงไม่ให้ hold ที่เปิด RESET กด Yes ต่อเอง และไม่ให้การแตะ Yes ครั้งเดียวกลายเป็น OK ด้วย

สิ่งที่ล้าง:

- `/smartlock` บน microSD รวม database, verifier, backup, log และ export ในโฟลเดอร์นี้
- Owner และ Devices ที่ลงทะเบียน
- SmartLock configuration ใน NVS
- AP password และ STA SSID/password ที่บันทึก
- test-state namespace และ Wi-Fi credentials ที่ SDK บันทึก

ค่าคาลิเบรตสัมผัสยังเก็บไว้เป็นค่าฮาร์ดแวร์ ไม่ต้องจิ้มคาลิเบรตจอใหม่หลัง reset

**การยืนยันบนจอจริงแทนการยืนยันสิทธิ์ Owner ผ่านเว็บตามคำขอผู้ใช้ล่าสุด** ผู้ที่เข้าถึงจอสามารถทำ physical reset ตามลำดับนี้ได้ ไม่มีการตรวจ credential ในเส้นทาง physical OK ส่วน reset web routes ไม่ได้ลงทะเบียนแล้ว

reset จะล็อกแม่เหล็กก่อนล้างข้อมูล ไม่มี unlock call ใน FactoryResetController หากการล้าง storage ล้มเหลวจะรายงานข้อผิดพลาดและยังอยู่ LOCKED ไม่ควรถือว่าล้างสำเร็จ

ข้อความไทยใช้ monochrome bitmap ใน PROGMEM เพื่อแสดงสระ/วรรณยุกต์ได้บน TFT_eSPI ที่ไม่ได้มีฟอนต์ไทยในตัว ภาพตัวอย่าง 320×480 ถูกตรวจแล้ว แต่ไม่เท่ากับการยืนยันภาพบน TFT จริง

## 6. ความปลอดภัยของ ACCESS

AccessController ตรวจ:

- Access session ถูกประเภท ยังไม่หมดอายุ และยังไม่ถูกใช้
- config และ SD database integrity
- device verifier ถูกต้อง
- Device ACTIVE และ Owner ACTIVE
- lock state และระยะเวลาปลดล็อกถูกต้อง

จากนั้น consume Access session ก่อนเรียก `LockController.unlock()` เพียงครั้งเดียว

LockController ใช้ ESP one-shot timer สำหรับ relock และมี loop เป็น backup; boot เริ่ม GPIO22 HIGH = LOCKED ระยะเวลาปลดล็อกมาจาก config (default 10 วินาที) คำขอระหว่าง unlocked ไม่ขยายเวลา

## 7. การแบ่งงาน

ผู้ควบคุมหลักรับผิดชอบ state machine, GPIO22, reset logic, authorization, session/replay, storage และการตัดสิน PASS/FAIL

Luna High ทำงานจำกัดขอบเขตด้าน UI:

- Owner bootstrap UI เดิม
- Automatic Owner handoff UI
- Reset web UI เดิม (ถูกแทนที่ด้วย physical UI แล้ว)
- Physical reset display และ Thai bitmap assets

ผลของ subagent ไม่ถูกใช้อนุมัติ security หรือ Phase PASS ด้วยตัวเอง

## 8. หลักฐาน software/runtime

- PlatformIO build ผ่าน
- Flash COM6 ผ่านและ image SHA verified
- Flash usage ล่าสุดประมาณ 74.7%, static RAM ประมาณ 37.4%
- JavaScript syntax ของ embedded pages ผ่านในการตรวจที่ทำก่อนหน้า
- LAN API preflight ก่อนการ reset: health/status/Access/bootstrap pages ตอบสนอง; unknown Access, missing credential, unauthenticated bootstrap และ unknown bootstrap ถูกปฏิเสธ
- session/state-machine self-test ใน firmware ผ่าน รวม Management hold, Reset selection และ session one-time-use

Serial หลัง flash สี SETUP รุ่นล่าสุด:

```text
LockController: LOCKED
LOCK TIMER: OK
Configured: NO
Owner exists: NO
TFT: OK
Touch calibration loaded
Touch: OK
SD: OK (30000 MB)
SESSION TEST: PASS
AP: OK SSID SmartLock-F0A4 IP 192.168.4.1
DNS: OK
HTTP: OK
HEAP: free=139032 min=138756
READY
```

นี่เป็นสถานะที่อ่านได้จาก Serial ล่าสุด ไม่ใช่หลักฐานว่าผู้ใช้ยืนยัน physical reset ผ่านครบทุกขั้นแล้ว

## 9. Git และไฟล์สำคัญ

Commit ล่าสุดที่ตรวจได้: `313dbba — Phase 5: first owner setup and protected AP`

งาน Phase 6/6A/7 และ UI/reset ล่าสุดยังเป็น working-tree changes จำนวนมาก **ยังไม่มี commit/tag ที่ยืนยัน Phase 7 PASS** ต้องระวังอย่า reset/checkout ทับงานเหล่านี้

ไฟล์สำคัญ:

- `src/app/AppStateMachine.cpp`, `AppState.h`
- `src/hardware/TouchManager.cpp`, `DisplayManager.cpp`, `ThaiResetBitmaps.h`
- `src/app/AccessController.cpp`, `FactoryResetController.cpp`, `EnrollmentManager.cpp`
- `src/qr/QrManager.cpp`, `SessionManager.cpp`
- `src/network/NetworkManager.cpp`, `WebServerManager.cpp`
- `src/web/WebAssets.h`
- `src/storage/StaSecrets.cpp`, `AuthStore.cpp`

มีโค้ด/asset จาก flow เก่าที่ไม่ได้ใช้งานบางส่วน เช่น Reset web asset/handler และ branch Reset QR ที่ถูก return ก่อนถึงใน renderer ควร review/cleanup ภายหลังโดยไม่ทำให้ flow ปัจจุบันเสีย

## 10. งานที่ยังต้องยืนยันก่อนประกาศ PASS

1. ลงทะเบียน Owner ใหม่จาก SETUP QR
2. ยืนยัน SETUP น้ำเงิน, ACCESS ขาว, MANAGEMENT เหลือง และ QR ขาวสแกนได้จริง
3. ทดสอบ hold → Management, release → hold → Reset โดยไม่เกิด repeated events
4. Reset ไม่มี QR; No กลับ Access; Yes เปิด OK แยก; นิ้วเดิมไม่กดยืนยันข้ามหน้าต่อเอง
5. เมื่อผู้ใช้ตั้งใจยืนยัน OK ตรวจ microSD/NVS กลับ initial setup และแม่เหล็กยังล็อก
6. ตั้ง Wi-Fi บ้าน, สลับโทรศัพท์, เปิด automatic handoff link โดยไม่มี registration form รอบสอง
7. fresh ACCESS QR → Owner recognized → แม่เหล็กปล่อยจริง → timed relock
8. replay URL เดิมต้องไม่ปลดล็อกอีก
9. ยืนยัน database persistence หลัง reboot และไม่เกิด duplicate Owner

**คำสั่งสำหรับ GPT ที่รับช่วง:** อ่านสถานะนี้และตรวจ source ปัจจุบันก่อนเปลี่ยนงาน อย่าอ้างว่า physical tests ผ่านจากการ compile/Serial เพียงอย่างเดียว อย่าประกาศ Phase 7 PASS หรือไปเฟสถัดไปจนมีผลยืนยันครบ และรักษางานที่ยังไม่ commit ทั้งหมดไว้

## Superseding acceptance
The user reported Phase 7 physical PASS on 2026-09-26. The current authoritative status and evidence are in PHASE_07_OWNER_ACCESS_REPORT.md; earlier FAIL/pending statements in this historical report are superseded.
