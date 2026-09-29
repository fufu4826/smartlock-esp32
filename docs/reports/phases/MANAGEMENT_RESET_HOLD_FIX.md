# Management-to-reset hold regression fix

Date: 2026-09-26

User reported that holding on Management did not open the red reset screen.

## Cause

TouchManager emits SingleTap on debounced finger-down so the first touch can wake Access immediately. AppStateMachine handled that press while in Management by switching to Access. The later Hold event therefore returned to Management instead of entering Reset.

## Change

Management now defers its short-tap transition until TapReleased. A Hold preserves the starting Management state and enters ResetRequest. TouchManager still emits at most one Hold per contact and suppresses TapReleased after a hold. No choice or factory reset is invoked by this fix. Reset still requires Yes, release, and a separate OK press. No returns to Access.

## Verification

The host test compiles the production TouchManager and AppStateMachine. PASS: immediate Access wake; first hold enters Management; continued hold does not repeat; release and second hold enter Reset; continued hold/release do not confirm; No returns Access; short Management tap returns Access only on release; Yes reaches the separate confirmation state; Setup remains unchanged.

An initial host build was blocked by the inherited Windows PATH. The runner uses the same minimal compiler environment as the existing project test runners; the subsequent compile/test passed.

COM6 upload and post-boot preservation checks are recorded after deployment. Physical visual verification remains required: hold to Management, lift finger, hold again to the red reset prompt, choose No. Do not press the destructive Yes/OK sequence during this regression.

The existing Owner Access migration checkpoint remains pending. This touch fix does not establish a physical Access PASS.

The first on-device boot self-test correctly surfaced a stale transition expectation (Management press was still expected to enter Access immediately). The test now asserts that press preserves Management, short release enters Access, and press followed by Hold enters Reset. Session/token tests were retained; no failed assertion was removed or bypassed.

Final COM6 build/upload PASS (87.58 s; image 1,228,880 bytes; flash 1,222,309 bytes; RAM 108,412 bytes). Final monitored reboot: SESSION TEST PASS; IDENTITY MODEL PASS; one active D000001/gugy Owner; OWNER_VERIFIER_PRESERVED=1; configured YES; touch calibration loaded; GPIO22 LOCKED; STA automatically connected at 192.168.1.179. USB inspection: free heap 134,788 bytes, minimum 118,872 bytes. No Factory Reset performed. Physical red-screen appearance remains awaiting user confirmation.
