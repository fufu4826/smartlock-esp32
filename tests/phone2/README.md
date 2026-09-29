# Phone2 controller host tests

Run with `python tests/phone2/run_phone2_tests.py`.

Run `node tests/phone2/enroll_browser_test.js` for production enrollment-JavaScript tests in independent fake browser storage contexts. These use WebCrypto but no HTTP endpoint, real credential or board. They verify independent generated credentials, Thai name submission, no Admin secret in requests, expired-grant hiding and the UTF-8 byte limit.

The runner stages temporary copies of the production `EnrollmentManager.cpp` and `AccessController.cpp`, replacing only their include directives so the real method bodies compile against isolated in-memory host fakes. It also compiles the production `SessionManager.cpp` and `RecordCodec.cpp` unchanged, with deterministic entropy supplied by an SDK fake. Thai UTF-8 name validation is exercised by the production codec. No firmware build or flash, real lock output, ESP32 storage, or database path is used.

The auth fake stores supplied test credentials in process memory and compares them as strings. It exercises controller credential binding and independence between devices, but it does not exercise or claim the production credential hashing implementation. The production session manager receives deterministic host entropy. The access tests call the production controller with a lock fake and verify the resulting requested duration, role/network decisions, and audit user ID.

Coverage includes production session type/expiry/consume boundaries; Owner/Admin/User/Guest management boundaries; unknown-role denial; Thai user/device names; independent per-device credentials; enrollment replay and expiry; issuer revocation while enrollment is pending; user disable/reenable without restoring an individually revoked device; state persistence across controller recreation; AP versus home-LAN access; audit attribution; invalid config and lock timer rejection; and storage integrity/read failures. The lock fake never drives GPIO or other hardware.
