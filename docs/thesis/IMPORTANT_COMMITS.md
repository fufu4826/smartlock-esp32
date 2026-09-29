# Important commits (Git-verified)

Every hash, date, subject, and changed-path summary below comes from the current local Git history. “Why it mattered” is limited to the commit subject, file changes, and linked archived reports; no unsupported implementation conclusions are added.

## 4439bdb — Checkpoint Google Sheets implementation before removal
- Full hash: `4439bdbab8ab0260175891b0e45c6e079613f8ce`
- Date: 2026-09-27T21:44:18+07:00
- Changed paths: docs/phase_reports/GOOGLE_SHEETS_ARCHIVE_CHECKPOINT.md
- Git summary: 1 file changed, 12 insertions(+)
- Why it matters: the commit records the milestone stated by its subject and files; use it to anchor the documented implementation/history at this point.
- Chapter support: Chapter 3 (design/development) and/or Chapter 4 (implementation/evidence), depending on the listed files. See the matching phase report and evidence under `07_PHASE_REPORTS_COMPLETE/` or architecture notes under `06_ARCHITECTURE_AND_SECURITY/`.

## b6db664 — Remove Google Sheets integration
- Full hash: `b6db664e6326eb8d62c45a8317beef6eabf8584c`
- Date: 2026-09-27T21:57:03+07:00
- Changed paths: README.md, cloud/google_receiver/Code.gs, cloud/google_receiver/Code.test.js, cloud/google_receiver/README.md, cloud/google_receiver/appsscript.json, cloud/google_receiver/publisher_pages.test.js, docs/PRODUCT_SCOPE.md, docs/design/GOOGLE_SHEETS_ARCHITECTURE.md, docs/design/GOOGLE_SHEETS_FREE_ONLY_ARCHITECTURE.md, docs/design/GOOGLE_SHEETS_ONE_CLICK_ARCHITECTURE.md, docs/phase_reports/GOOGLE_FREE_PHASE_F1_D1_WORKER_REPORT.md, docs/phase_reports/GOOGLE_PHASE_01_PROTOCOL_BACKEND_REPORT.md (additional paths in Git export)
- Git summary: 66 files changed, 1848 insertions(+), 3584 deletions(-)
- Why it matters: the commit records the milestone stated by its subject and files; use it to anchor the documented implementation/history at this point.
- Chapter support: Chapter 3 (design/development) and/or Chapter 4 (implementation/evidence), depending on the listed files. See the matching phase report and evidence under `07_PHASE_REPORTS_COMPLETE/` or architecture notes under `06_ARCHITECTURE_AND_SECURITY/`.

## 064518a — Implement RAM-only LINE security notifications
- Full hash: `064518abc55002a121a03c7b848a2e635a9afb78`
- Date: 2026-09-27T22:47:44+07:00
- Changed paths: docs/design/LINE_SECURITY_NOTIFICATIONS_ARCHITECTURE.md, docs/phase_reports/LINE_NOTIFICATIONS_IMPLEMENTATION_REPORT.md, docs/phase_reports/evidence/line_v1/build.txt, docs/phase_reports/evidence/line_v1/line-host-tests.txt, docs/phase_reports/evidence/line_v1/management-browser.json, docs/phase_reports/evidence/line_v1/pre-flash.txt, docs/phase_reports/evidence/line_v1/regression.json, docs/phase_reports/evidence/line_v1/regression.txt, docs/phase_reports/evidence/line_v1/tls-root.json, src/app/AccessController.cpp, src/app/EnrollmentManager.cpp, src/app/FactoryResetController.cpp (additional paths in Git export)
- Git summary: 45 files changed, 2052 insertions(+), 177 deletions(-)
- Why it matters: the commit records the milestone stated by its subject and files; use it to anchor the documented implementation/history at this point.
- Chapter support: Chapter 3 (design/development) and/or Chapter 4 (implementation/evidence), depending on the listed files. See the matching phase report and evidence under `07_PHASE_REPORTS_COMPLETE/` or architecture notes under `06_ARCHITECTURE_AND_SECURITY/`.

## 3fc128d — Add Owner-gated USB LINE provisioning and remove web credential entry
- Full hash: `3fc128d8832f51ca8d760f301a68e34c860b3dbb`
- Date: 2026-09-28T00:59:16+07:00
- Changed paths: docs/design/LINE_ONLY_OWNER_PROVISIONING_ARCHITECTURE.md, docs/phase_reports/LINE_OWNER_PROVISIONING_V2_COMPLETE_REPORT.md, docs/phase_reports/evidence/line_v2/build.txt, docs/phase_reports/evidence/line_v2/core-tests.txt, docs/phase_reports/evidence/line_v2/management-browser.json, docs/phase_reports/evidence/line_v2/nvs-read-before.txt, docs/phase_reports/evidence/line_v2/pre-flash.txt, docs/phase_reports/evidence/line_v2/regression.json, docs/phase_reports/evidence/line_v2/regression.txt, docs/phase_reports/evidence/line_v2/source-review.json, src/events/Diagnostics.cpp, src/main.cpp (additional paths in Git export)
- Git summary: 27 files changed, 1496 insertions(+), 177 deletions(-)
- Why it matters: the commit records the milestone stated by its subject and files; use it to anchor the documented implementation/history at this point.
- Chapter support: Chapter 3 (design/development) and/or Chapter 4 (implementation/evidence), depending on the listed files. See the matching phase report and evidence under `07_PHASE_REPORTS_COMPLETE/` or architecture notes under `06_ARCHITECTURE_AND_SECURITY/`.

## 48c653e — Authorize scoped LINE USB setup through physical Admin
- Full hash: `48c653e46dc948aacab307f9082cdfea47f4603d`
- Date: 2026-09-28T02:40:25+07:00
- Changed paths: docs/design/LINE_ONLY_OWNER_PROVISIONING_ARCHITECTURE.md, docs/phase_reports/LINE_OWNER_PROVISIONING_V2_COMPLETE_REPORT.md, docs/phase_reports/evidence/line_v2_physical/admin-preview.png, docs/phase_reports/evidence/line_v2_physical/build.txt, docs/phase_reports/evidence/line_v2_physical/core-tests.txt, docs/phase_reports/evidence/line_v2_physical/host-tests.txt, docs/phase_reports/evidence/line_v2_physical/local-session-check.json, docs/phase_reports/evidence/line_v2_physical/management-browser.json, docs/phase_reports/evidence/line_v2_physical/nvs-read-before.txt, docs/phase_reports/evidence/line_v2_physical/pre-flash.txt, docs/phase_reports/evidence/line_v2_physical/preservation-regression.json, docs/phase_reports/evidence/line_v2_physical/preservation-regression.txt (additional paths in Git export)
- Git summary: 30 files changed, 357 insertions(+), 97 deletions(-)
- Why it matters: the commit records the milestone stated by its subject and files; use it to anchor the documented implementation/history at this point.
- Chapter support: Chapter 3 (design/development) and/or Chapter 4 (implementation/evidence), depending on the listed files. See the matching phase report and evidence under `07_PHASE_REPORTS_COMPLETE/` or architecture notes under `06_ARCHITECTURE_AND_SECURITY/`.

## 14645a9 — Give isolated LINE TLS worker sufficient handshake stack
- Full hash: `14645a9b0e6707ed9f7124a9686c6adf24bff659`
- Date: 2026-09-28T03:20:46+07:00
- Changed paths: src/notifications/LineNotifications.cpp
- Git summary: 1 file changed, 3 insertions(+), 1 deletion(-)
- Why it matters: the commit records the milestone stated by its subject and files; use it to anchor the documented implementation/history at this point.
- Chapter support: Chapter 3 (design/development) and/or Chapter 4 (implementation/evidence), depending on the listed files. See the matching phase report and evidence under `07_PHASE_REPORTS_COMPLETE/` or architecture notes under `06_ARCHITECTURE_AND_SECURITY/`.

## f750bf8 — Record private LINE provisioning and live TLS recovery evidence
- Full hash: `f750bf85b7757d6fabe84c1364edd51467568543`
- Date: 2026-09-28T03:22:23+07:00
- Changed paths: docs/phase_reports/LINE_OWNER_PROVISIONING_V2_COMPLETE_REPORT.md, docs/phase_reports/evidence/line_v2_physical/line-tls-stack-fix.txt, docs/phase_reports/evidence/line_v2_physical/real-line-provisioning.txt
- Git summary: 3 files changed, 68 insertions(+)
- Why it matters: the commit records the milestone stated by its subject and files; use it to anchor the documented implementation/history at this point.
- Chapter support: Chapter 3 (design/development) and/or Chapter 4 (implementation/evidence), depending on the listed files. See the matching phase report and evidence under `07_PHASE_REPORTS_COMPLETE/` or architecture notes under `06_ARCHITECTURE_AND_SECURITY/`.

## c5106ac — Confirm Owner receipt and complete LINE acceptance
- Full hash: `c5106ac94d894f088e06c1af3cf69851db162f50`
- Date: 2026-09-28T03:26:07+07:00
- Changed paths: docs/phase_reports/LINE_OWNER_PROVISIONING_V2_COMPLETE_REPORT.md
- Git summary: 1 file changed, 4 insertions(+)
- Why it matters: the commit records the milestone stated by its subject and files; use it to anchor the documented implementation/history at this point.
- Chapter support: Chapter 3 (design/development) and/or Chapter 4 (implementation/evidence), depending on the listed files. See the matching phase report and evidence under `07_PHASE_REPORTS_COMPLETE/` or architecture notes under `06_ARCHITECTURE_AND_SECURITY/`.

## 2b4fcf2 — Add protected same-origin Wi-Fi fallback for Access and Management
- Full hash: `2b4fcf217c55b9a73e756bd14ac1522b0750007f`
- Date: 2026-09-28T03:42:36+07:00
- Changed paths: docs/design/LOCAL_CANONICAL_FALLBACK.md, docs/phase_reports/LOCAL_CANONICAL_FALLBACK_REPORT.md, docs/phase_reports/evidence/local_fallback/build.json, docs/phase_reports/evidence/local_fallback/lan-health-before.json, docs/phase_reports/evidence/local_fallback/layout-preview.png, docs/phase_reports/evidence/local_fallback/mobile-origin.json, docs/phase_reports/evidence/local_fallback/pre-flash.txt, src/app/AppStateMachine.h, src/app/LocalFallbackTouch.h, src/hardware/LocalFallbackBitmaps.h, src/main.cpp, src/network/CanonicalOrigin.cpp (additional paths in Git export)
- Git summary: 26 files changed, 810 insertions(+), 10 deletions(-)
- Why it matters: the commit records the milestone stated by its subject and files; use it to anchor the documented implementation/history at this point.
- Chapter support: Chapter 3 (design/development) and/or Chapter 4 (implementation/evidence), depending on the listed files. See the matching phase report and evidence under `07_PHASE_REPORTS_COMPLETE/` or architecture notes under `06_ARCHITECTURE_AND_SECURITY/`.

## c229d41 — Preserve mDNS responder and disable modem sleep for availability
- Full hash: `c229d4113e5425690e30c33fc80c68fb6fc92287`
- Date: 2026-09-28T03:48:52+07:00
- Changed paths: src/network/CanonicalOrigin.cpp, src/network/NetworkManager.cpp, tests/network/canonical_origin_mocks/ESPmDNS.h, tests/network/canonical_origin_test.cpp, tests/network/mocks/WiFi.h, tests/network/network_manager_test.cpp, tests/network/network_manager_test_fakes.cpp
- Git summary: 7 files changed, 53 insertions(+), 33 deletions(-)
- Why it matters: the commit records the milestone stated by its subject and files; use it to anchor the documented implementation/history at this point.
- Chapter support: Chapter 3 (design/development) and/or Chapter 4 (implementation/evidence), depending on the listed files. See the matching phase report and evidence under `07_PHASE_REPORTS_COMPLETE/` or architecture notes under `06_ARCHITECTURE_AND_SECURITY/`.

## 870ec7f — Give LINE Add Friend its own Management page and remove TFT fallback button
- Full hash: `870ec7f6c2c45f000ba42facbed5c9da8a1ec2f6`
- Date: 2026-09-28T03:58:02+07:00
- Changed paths: docs/phase_reports/evidence/line_friend_page/management-browser.json, docs/phase_reports/evidence/line_friend_page/mobile-friend-page.png, docs/phase_reports/evidence/line_friend_page/pre-flash.txt, src/main.cpp, src/web/WebAssets.h, tests/admin/line_management_browser_test.py
- Git summary: 6 files changed, 46 insertions(+), 38 deletions(-)
- Why it matters: the commit records the milestone stated by its subject and files; use it to anchor the documented implementation/history at this point.
- Chapter support: Chapter 3 (design/development) and/or Chapter 4 (implementation/evidence), depending on the listed files. See the matching phase report and evidence under `07_PHASE_REPORTS_COMPLETE/` or architecture notes under `06_ARCHITECTURE_AND_SECURITY/`.

## 979fc74 — Move LINE maintenance to Owner and shared Admin PIN; release HTTP buffers
- Full hash: `979fc74405944e28375d92bdcb870b5aed1c1f6a`
- Date: 2026-09-28T04:32:11+07:00
- Changed paths: docs/phase_reports/LINE_WEB_ADMIN_DELIVERY_REPORT.md, docs/phase_reports/evidence/line_web_admin/management-browser.json, docs/phase_reports/evidence/line_web_admin/mobile-friend-page.png, docs/phase_reports/evidence/line_web_admin/pre-flash.txt, lib/SmartLockWebServer/src/Parsing.cpp, lib/SmartLockWebServer/src/WebServer.cpp, src/app/AdminTouchRouter.h, src/app/FirstOwnerSetup.cpp, src/app/PhysicalAdmin.cpp, src/app/PhysicalAdmin.h, src/events/Diagnostics.cpp, src/hardware/AdminDisplay.cpp (additional paths in Git export)
- Git summary: 37 files changed, 729 insertions(+), 751 deletions(-)
- Why it matters: the commit records the milestone stated by its subject and files; use it to anchor the documented implementation/history at this point.
- Chapter support: Chapter 3 (design/development) and/or Chapter 4 (implementation/evidence), depending on the listed files. See the matching phase report and evidence under `07_PHASE_REPORTS_COMPLETE/` or architecture notes under `06_ARCHITECTURE_AND_SECURITY/`.

## d1e5ce3 — fix: harden admin pin bootstrap and emergency unlock gating
- Full hash: `d1e5ce38058790a973ce90c1addf33ef206ce4cd`
- Date: 2026-09-28T12:09:16+07:00
- Changed paths: src/main.cpp, src/security/AdminPin.cpp, src/security/AdminPin.h, tests/admin/admin_test.cpp, tests/admin/admin_transition_test.cpp, tests/admin/setup_backend_test.py
- Git summary: 6 files changed, 47 insertions(+), 10 deletions(-)
- Why it matters: the commit records the milestone stated by its subject and files; use it to anchor the documented implementation/history at this point.
- Chapter support: Chapter 3 (design/development) and/or Chapter 4 (implementation/evidence), depending on the listed files. See the matching phase report and evidence under `07_PHASE_REPORTS_COMPLETE/` or architecture notes under `06_ARCHITECTURE_AND_SECURITY/`.

Final accepted ref check at archive time: `smartlock-final-accepted-2026-09-28` resolved to `d1e5ce38058790a973ce90c1addf33ef206ce4cd`, equal to accepted commit `d1e5ce38058790a973ce90c1addf33ef206ce4cd`.