# Single-identity migration checkpoint

Date: 2026-09-26. User-requested architecture change supersedes the earlier User -> Devices model. Google deployment remains paused; Phone #3 remains deferred.

## Status

- Implementation / host acceptance: PASS (Sol review).
- COM6 deployment / USB-verified live persistence: PASS after two normal reboots. Windows HTTP/mDNS probe timed out; phone reachability/physical Access remains the next checkpoint.
- Existing Owner validity: PASS by user physical confirmation; Owner unlock after revocation remains pending.
- Simplified real Phone #2 enrollment/access/timed relock: PASS by user confirmation. Final revoke/Owner-after-revoke physical regression: PENDING.
- No Factory Reset, re-registration, NVS clearing, or live test identity creation was performed by this migration task.

## Real database inspected before migration

The USB read-only diagnostic found:

| Old record | Name | Role/status | Relationship |
|---|---|---|---|
| U000001 | gugy | OWNER / ACTIVE | D000001 |
| D000001 | gugy | ACTIVE | One existing verifier |
| U000002 | Boom | USER / ACTIVE | No device or verifier |

The unbound Boom record is the incomplete recent experiment. It is omitted from the new authoritative identities. It remains in immutable history; no enrolled identity is deleted. The real Owner name is **gugy** (the user's `guy` example was illustrative).

## New authority

`/smartlock/db/identities.rec` holds I1 CRC-protected records with D###### ID, normalized name, role and ACTIVE/REVOKED status. The ID references the unchanged salted verifier format in `/smartlock/db/auth.rec`. `/smartlock/db/identity-mode.rec` marks the committed identity model. Exactly one Owner is allowed, always ACTIVE D000001.

Normal Access, Management, enrollment, revocation and configured-state validation use IdentityStore. There is no User-to-Device lookup. Legacy users.rec/devices.rec remain read-only historical migration inputs and are not consulted after the new mode commits. Their loss or corruption cannot replace the identity authority.

The current Owner becomes D000001 / gugy / OWNER / ACTIVE. Migration never calls AuthStore.saveFirst, addDevice, ConfigStore.save, NVS clear, network credential save, or Factory Reset.

## Transaction / recovery

Before new database writes, migration creates and byte-verifies these SD snapshots:

- `/smartlock/backups/identity-v1/users.rec`
- `/smartlock/backups/identity-v1/devices.rec`
- `/smartlock/backups/identity-v1/auth.rec`

It validates the proposed identities, writes an intent marker, writes and validates identities, checks auth.rec is byte-for-byte unchanged, and commits the mode marker last. Existing users/devices/auth files are never rewritten by migration. Duplicate normalized names, multiple Owner devices, unknown relationships, missing verifiers, or orphan verifiers in the legacy source stop migration before new writes.

A partial backup/intent/identity artifact prevents automatic re-migration. A missing or corrupt committed identity database cannot fall back to users.rec or an older .bak file. The board fails closed and preserves all recovery evidence. Recovery requires inspecting the exact failed stage and restoring a consistent verified snapshot; do not erase the SD/NVS or reset merely to bypass the problem. After subsequent legitimate enrollments, restoring an old snapshot would lose revocations; it must never be automatic.

## Enrollment / permissions

- Thai Management shows one flat **อุปกรณ์ที่ได้รับสิทธิ์** list.
- Owner provides **ชื่อ** and **สิทธิ์**; no separate User is created.
- Name normalization trims surrounding ASCII whitespace, rejects empty/control/delimiter/malformed UTF-8 names and limits UTF-8 to 40 bytes. Comparison is case-sensitive; no Unicode NFC or case-folding is claimed. ACTIVE and REVOKED names remain reserved.
- Owner may grant Admin/User/Guest. Admin may grant/revoke only User/Guest. No enrollment can grant Owner; D000001 cannot be revoked.
- One pending cryptographically random 120-second grant is held in RAM, bound to normalized name, role, issuer and session. A new grant replaces the preceding grant. Issuer state/role and name availability are checked again at redemption.
- Enrollment QR appears only on the Owner web page. No ENROLL TFT rendering or state remains in the main loop.
- Invited browser sees assigned name/role and one Register button. It generates an independent 256-bit opaque credential; completion accepts only session + credential.
- An already registered browser is asked to use another browser/phone, preserving its existing credential. Server also rejects reusing an existing credential for another identity. Browser storage is not hardware attestation.
- The grant is consumed before persistence. Auth is saved before the identity; an interrupted enrollment can leave an unusable orphan verifier, never an authorized identity. IDs skip existing verifier IDs. No plaintext credential is written or printed by the ESP32.
- Canonical hostname and browser storage keys remain compatible. Old Owner bootstrap/verify and separate User mutation endpoints are retired (410), with no way to create a second Owner.
- AP remains Owner setup/recovery; enrollment requires actual LAN interface. Access checks session, healthy DB/config, credential, ACTIVE identity, role/network, consumption, then existing LockController/timed relock.

## Event protocol mapping

Phase 11/12 schema remains unchanged: new identity actions use `user_id = ""`, `device_id = D######`. No synthetic human User is created. Old events retain their original U/D values. Empty user_id is already accepted by the local event validator and receiver contract. Names/roles are not added to the existing protocol. Google deployment stays paused; no claim of new cloud E2E validation.

## Evidence / limits

- Production IdentityStore, AtomicFileStore, legacy codecs/stores, SessionManager, EnrollmentManager and AccessController compiled in host tests: **336 assertions PASS**, including **20 injected migration SD mutation failures**.
- Cases include unchanged Owner ID/verifier mapping, unbound test exclusion, boot idempotence, independent authority from legacy files, name normalization/collision, unauthorized enrollment, Owner escalation rejection, independent credential binding, grant replacement/expiry/replay, selected revocation, simulated reboot persistence, Owner still accepted, non-Owner AP Access denied, corrupt/missing database fail-closed, and unchanged originals across migration failures.
- Host AuthStore, clock, entropy, lock and SD are fakes. These tests do not prove physical lock motion, actual cryptography or real SD power-loss behavior. Production verifier preservation is checked separately on the board by comparing D000001 salt/hash and admin verifier to the byte-verified backup, without printing them.
- Browser VM tests run production JS with separate storage contexts: assigned name/role, no extra name/password prompt, exact completion fields, distinct random credentials, retained Owner storage, duplicate-name Thai error and registered-browser guard PASS.
- Six active web assets pass JS syntax checks. Retired bootstrap/verify assets are removed.
- Unchanged production NetworkManager: **101 assertions PASS**.
- Sol independently reviewed authorization, migration order/failure paths, one-Owner invariant, credential mapping, role constraints, replay/revocation, event mapping and GPIO22 ownership. Luna provided bounded UI and test harness support; Luna did not approve the migration.

## Live deployment evidence

`pio run -t upload --upload-port COM6` succeeded, flashed image hashes verified. Firmware image: 1,228,784 bytes; flash usage 1,222,209 / 1,310,720 (93.2%); static RAM 108,412 / 327,680 (33.1%). Only normal firmware partitions were flashed; no full-chip erase or NVS partition erase.

Two subsequent normal COM6 reboots reported:

```text
LockController: LOCKED
GPIO22: LOCKED
LOCK TIMER: OK
Configured: YES
Owner exists: YES
Touch calibration loaded
SD: OK (30000 MB)
SESSION TEST: PASS
IDENTITY MODEL: PASS OWNER_VERIFIER_PRESERVED=1
IDENTITY DB: PASS identities=1 owners=1 active=1 revoked=0
CANONICAL: http://smartlock-04225a0ff0a4.local MDNS: OK
HTTP: OK
STA saved config: CONNECTING
STA: CONNECTED IP 192.168.1.179
```

Read-only `DIAG_AUTH` after the second reboot:

```text
IDENTITY META: id=D000001 name=gugy role=0 status=0 verifier=1
AUTH INSPECT: valid=1 identities=1 owners=1 verifiers=1 OWNER_VERIFIER_PRESERVED=1 heap=134792 min_heap=116060
```

`OWNER_VERIFIER_PRESERVED=1` compares the real current D000001 salt/hash and admin verifier with the pre-migration backup; no verifier bytes are printed. The first migration ran on the automatic post-flash boot before the monitor attached. Subsequent boot and diagnostic checks confirm committed mode, intact backup and Owner mapping; the initial migration success log is not claimed as captured.

Cloud remains disabled/unconfigured. The existing missing `a`/`b` cloud NVS slot messages precede the disabled cloud status; configured Owner/Wi-Fi checks pass. No panic, watchdog or unintended unlock was observed during monitored reboots. Physical magnet motion/holding force is not inferred from Serial.

Windows (192.168.1.145) timed out reaching HTTP and direct mDNS; its neighbor entry for the board's correct MAC was Unreachable. ESP32 Serial independently showed successful STA association/IP and canonical mDNS initialization. This is recorded as the existing Windows-side connectivity regression, not evidence of lost Owner state or lost saved Wi-Fi. Current phone-side reachability must be confirmed in the next requested Access test; no fresh phone PASS is invented.

The I1 schema does not add fabricated created_at/last_seen values. Timestamped actions continue in the existing audit log; per-identity timestamp fields are not part of this migration.

## Required next checkpoint

Use the existing Owner phone/browser on home Wi-Fi. Single tap -> fresh ACCESS QR -> scan -> magnet releases -> timed relock. Do not Setup or register again. Only after this passes proceed to simplified Phone #2 enrollment/access/revoke. Phone #3 and broader physical regression remain on the Phase 14 checklist.


## Real Phone #2 acceptance — user confirmed 2026-09-26

User physically confirmed existing Owner validity; adding Boom; web enrollment QR generation; Phone #2 registration with no duplicate name/device prompt; independent credential; fresh Access QR unlock; timed automatic relock. These are user-supplied physical results, not inferred from build or Serial.

Read-only USB inspection after that confirmation:

| Identity | Actual stored name | Role | Status |
|---|---|---|---|
| D000001 | gugy | OWNER | ACTIVE |
| D000002 | boom | USER | ACTIVE |
| D000003 | Root | USER | ACTIVE |

Database valid, 3 identities, exactly 1 Owner, 3 verifiers, original Owner verifier preserved. Root is an additional existing identity observed in the live database; no assumption is made that this satisfies Phone #3 physical acceptance. Root is not the Owner/Admin despite its display name and is not a revoke target.

Final revoke checkpoint targets **D000002 / boom only** through authenticated Owner Management. The current Codex browser has no supplied Owner credential; the existing Owner phone must perform the authenticated revoke and the physical magnet observations. No authentication bypass or USB mutation command is added. Expected: boom REVOKED; fresh Phone #2 Access denied with magnet locked; existing Owner fresh Access still unlocks and relocks. D000001 and D000003 remain unchanged. Revoke results have not yet been reported.

Google deployment remains paused; Phone #3 remains deferred. Firmware/enrollment/access architecture unchanged during this acceptance-record update.

## User-reported Phone #2 revoke PASS; live persistence discrepancy (2026-09-26)

The user physically confirmed revoking only boom/D000002, seeing REVOKED, fresh Phone #2 Access denied with magnet LOCKED, gugy and Root unchanged, and Owner fresh Access unlock/timed relock. Record these as user-reported physical PASS.

However, read-only COM6 inspection immediately afterward reported all three identities ACTIVE, including boom status=0. A normal reboot then reported identities=3 owners=1 active=3 revoked=0. A second DIAG_AUTH confirmed D000002/boom status=0 (RecordStatus::Active), while gugy and Root remained ACTIVE and OWNER_VERIFIER_PRESERVED=1.

Therefore revocation persistence is NOT accepted yet. The user report and connected-board state conflict; neither is silently discarded. No Factory Reset, re-enrollment, identity mutation, firmware change, or automatic re-revocation was performed to hide this discrepancy. The reboot booted GPIO22 LOCKED, retained configured state/calibration and reconnected saved STA at 192.168.1.179. Google remains disabled/paused; Phone #3 deferred.

Next evidence needed: fresh Owner Management identity list after this reboot, specifically boom/D000002 status, to reconcile phone and USB views. Do not claim the earlier denial proves persistent revocation when the current SD record is ACTIVE.
