# Publishing console diagnostics

The PS4 PRX has an opt-in GitHub Device Flow uploader. A controller chord shows
a preview and a second press confirms the batch. Upload remains unvalidated on
the target console and is not automatic when recording stops.

## Planned authenticated flow

The project now has a registered GitHub OAuth App and a public Client ID
configuration in `include/pne/github_oauth.hpp`. The intended headless
authorization is GitHub Device Flow; see
[`GITHUB_DEVICE_FLOW.md`](GITHUB_DEVICE_FLOW.md).

The implementation includes PS4 HTTPS/TLS transport, Device Flow polling,
short-lived token handling, GitHub Contents API upload, and the controller
preview/confirmation flow. This is source-level implementation evidence only;
it does not establish that uploads work on firmware 14.02.

## Upload confirmation integrity

The pending confirmation records every eligible relative filename, byte count,
and SHA-256 of its contents. File ordering does not affect the snapshot. If a
name, size, digest, or eligible file set changes, the next press previews and
arms the changed batch instead of inheriting confirmation. Authorization start,
successful authorization, and disconnect invalidate any pending snapshot.

Immediately before upload, the worker re-enumerates and hashes the eligible
batch and requires an exact snapshot match. Each file is then read again and
its bytes are checked against the confirmed SHA-256 before any GitHub request
for that file is made. A mismatch fails the upload. This protects the bytes
sent from a same-size replacement between preview and upload; it is not a
firmware, transport, or on-console validation result.

## Privacy requirements

1. Stop the capture session.
2. Show the exact files and sizes that will be published.
3. Require explicit confirmation for each upload batch.
4. Never publish captures automatically when recording stops.
5. Never log or commit OAuth access/refresh tokens.
6. Treat every upload as public: this repository is public.

A successful cross-compile is not evidence that network authorization or
uploads work on PS4 firmware 14.02.
