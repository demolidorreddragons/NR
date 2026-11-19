# Publishing console diagnostics

The current PS4 PRX stores diagnostics locally. Console-to-GitHub upload is not
implemented in this build.

## Planned authenticated flow

The project now has a registered GitHub OAuth App and a public Client ID
configuration in `include/pne/github_oauth.hpp`. The intended headless
authorization is GitHub Device Flow; see
[`GITHUB_DEVICE_FLOW.md`](GITHUB_DEVICE_FLOW.md).

The remaining implementation includes PS4 HTTPS/TLS transport, Device Flow
polling, short-lived token handling, GitHub Contents API upload, and an
on-console review/confirmation screen. Until those pieces are implemented and
validated on the target console, do not assume the PS4 can upload.

## Privacy requirements

1. Stop the capture session.
2. Show the exact files and sizes that will be published.
3. Require explicit confirmation for each upload batch.
4. Never publish captures automatically when recording stops.
5. Never log or commit OAuth access/refresh tokens.
6. Treat every upload as public: this repository is public.

A successful cross-compile is not evidence that network authorization or
uploads work on PS4 firmware 14.02.
