# LINE Add Friend page and TFT cleanup

User confirmed previous Access/Management/LINE/unlock/relock flow working before this change.

Source commit:870ec7f. Add Friend now has a separate Owner-only page directly in Management navigation; notification settings remain under System. The public LINE URL remains validated and encoded; no token or recipient is exposed. Removed the TFT fallback footer and associated touch action so no invisible button remains. Automatic protected AP, DNS/mDNS and modem-sleep fixes are preserved.

Production mobile Chromium checks PASS, visual page reviewed; Admin81 checks and router/gesture regression PASS; production build PASS. Binary1235888 bytes, slot headroom74832, staticRAM113532. Application-only COM6 flash at0x10000, written hash verified. No reset/erase/partition change. NVS namespace comparisons all identical; private snapshots deleted. Boot identity database2 identities/1Owner/2active; PIN/SD/calibration healthy; GPIO22 LOCKED. Inert SD history23files18945bytes digest a63df454 unchanged, writesDISABLED. Notification/authorization/LockController source unchanged.

Post-reboot network checks recorded separately; no physical unlock or test notification triggered for this UI-only change.

Live /manage assets PASS: standalone page/navigation and friend anchor placement verified. /health200 after startup. Runtime limitation: post-startup LINE state5 ServiceError with largest contiguous heap34804 (free108772); engine40KB guard pauses outbound work. No fresh notification triggered; do not claim notification runtime PASS. This heap limitation was also observed before this UI flash, as pre-flash evidence shows state5. Two normal reboot observations retained; final lock remains HIGH. Physical rendering after this change not separately photographed.
