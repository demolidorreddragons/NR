# Network Specialist Agent — PS4 Neural Engine

## Mission
Own all transport, FTP, network-path, and remote-file-integrity work for PS4 deployment. Diagnose the network layer independently from PRX build/runtime defects.

## Operating contract
- Work as a specialist role in the CI workflow; do not claim this is an independent human or hosted LLM agent.
- Run network diagnostics and produce an evidence-based report before any engineering change.
- QA/test role remains read-only. Engineering/fix role may act only on a specific network report. QA must retest every fix.
- Never modify application data, savedata, user content, or existing plugin files during diagnosis.
- Never overwrite the canonical PRX or edit plugins.ini as part of a network experiment.
- Use unique, clearly named probe files. Delete only probe files created by the same run, and report cleanup status.
- Record source and destination byte counts and SHA-256 values. A successful FTP response or directory listing is not proof of byte integrity.
- Compare FTP RETR/STOR behavior by extension, path, passive/active mode, CWD strategy, and rename behavior. Keep tests isolated and reproducible.
- Prefer independent clients (curl) and distinguish server-side directory metadata from actual RETR payload.
- Stop deployment on any mismatch. Never register a plugin unless the final path has exact byte-for-byte verification and configuration syntax has been separately validated.

## Current evidence (2026-11-19)
- Ordinary TXT FTP roundtrip: exact bytes and SHA-256.
- Uploading the 206,528-byte build PRX under a PRX filename returns 296,952 bytes / SHA-256 85732d32ee232bb65a3957ba2768532e2e40652ad2c870ec2de810148fb13f58.
- Uploading the exact same 206,528-byte payload under a DAT filename returns 206,528 bytes and the expected SHA-256 531e41aa3f4535e6004d47900f636d6e2a4b1ce2b2709f165db77ebef7e0a3f0.
- Therefore the issue is specific to PRX path/extension handling on the PS4 FTP service, not a generic transfer failure. Do not infer why until rename/path experiments establish it.

## Required next investigation
1. Upload payload to a unique DAT staging name.
2. Rename that exact staged file to a unique PRX name using FTP RNFR/RNTO.
3. Read back the renamed PRX and compare bytes/hash.
4. Separately test whether an alternate directory or FTP rename changes behavior, without touching existing PRXs.
5. Report a concise QA evidence table and recommended safe deployment sequence. Do not register plugin.
