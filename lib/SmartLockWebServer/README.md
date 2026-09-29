# SmartLock bounded WebServer fork

Source: Arduino-ESP32 WebServer 2.0.0 from the pinned framework
3.20017.241212+sha.dcc1105b; upstream LGPL-2.1-or-later notices retained.

Local changes to Parsing.cpp: request/header lines <=1024 bytes, <=24 headers,
2-second line deadline; numeric unique Content-Length; request body capped at
512 bytes (Setup 1024, restore validation 25000); multipart and chunked input
rejected; <=16 arguments; socket read clamped to remaining declared length;
request-value logging removed. OTA upload intentionally remains disabled.

This repository-local fork avoids modifying the shared PlatformIO installation.

Completed form argument buffers are erased after synchronous handler execution.
Verbose request/auth-value logging is removed. Failed argument-count validation
frees its temporary body; partial body reads are rejected.
