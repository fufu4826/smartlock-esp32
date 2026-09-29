# Enrollment QR display regression

Date: 2026-09-26

The Owner reported that Create enrollment QR showed a LAN error while the phone was connected to home Wi-Fi and the authenticated identity list was visible.

## Root cause and reproduction

The asynchronous identityForm submit handler awaited the enrollment HTTP request and then used event.currentTarget.reset(). Native event dispatch clears currentTarget after the synchronous listener returns. A successful response therefore led to a null-reference exception before showEnrollment, which the generic catch mislabeled as LAN failure.

The browser-context test was extended to clear currentTarget immediately after dispatch while returning a valid successful enrollment response. Before the fix it failed: the QR panel remained hidden. After capturing the form before awaiting and using that stable reference, the same test passes and the QR image/panel/success message appear.

The generic error message no longer assumes LAN is the cause. Duplicate-name messaging remains distinct. No authorization, role, session, database, GPIO22, Wi-Fi, reset gesture, or credential code changed.

Production JS browser-context suite and syntax checks for all six active pages: PASS. Compile/flash and live preservation checks follow below. Successful real phone QR display remains a physical/browser checkpoint; the response used in the host test was synthetic.

After flashing, open a fresh Management QR in the existing Owner browser (old RAM sessions expire on reboot), enter Boom/User and create the QR again. No Setup, re-registration, or Factory Reset is needed.

Deployment: COM6 build/upload PASS, image hash verified (88.41 s). Post-flash USB inspection: database valid; exactly one identity D000001/gugy, one Owner, one verifier; OWNER_VERIFIER_PRESERVED=1. Free heap 134,652 bytes, minimum 121,792. No new live identity was created and no reset was performed. Real Owner-phone QR display still requires retry with a fresh Management session.
