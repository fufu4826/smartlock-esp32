# Manifest notes

`DOCUMENTATION_MANIFEST.csv` records every file already in the documentation archive outside this `manifest` directory, with archive-relative path, byte size, SHA-256, and top-level category. It excludes itself because a file cannot contain its own stable digest. The archive gate report and `COPY_HASH_VERIFICATION.csv` provide copy-source verification.

`CLEAN_RUNTIME_MANIFEST.csv` is intentionally pending the separate active-project cleanup/build gate. Sol should generate it only after the final runtime file set is settled.