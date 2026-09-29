# Firmware versions

| Folder | Source commit | SHA-256 | Status |
|---|---|---|---|
| `latest/firmware.bin` | `eaac772` (tag `smartlock-line-broadcast-accepted-2026-09-28`) | `546e96b03583eaf5405bbd7b1a6100cfee91937ba9047cc0026db4f4caa58e36` | **Current accepted prototype** (LINE broadcast) |
| `previous/firmware.bin` | `d1e5ce3` (tag `smartlock-final-accepted-2026-09-28`) | `084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9` | Original accepted release (fixed-recipient LINE push) |

## Flash (application only; keeps NVS, SD and calibration)

```bash
python -m esptool --chip esp32 --port COM6 --baud 460800 write_flash 0x10000 firmware.bin
python -m esptool --chip esp32 --port COM6 --baud 460800 verify_flash 0x10000 firmware.bin
```

**Known accepted limitation of `eaac772`:** a physical TFT Admin Emergency Unlock releases the lock, but no LINE notification is received.
