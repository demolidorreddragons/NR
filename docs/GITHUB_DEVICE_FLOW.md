# GitHub authorization for the PS4

The registered OAuth App uses GitHub Device Flow. The public Client ID is
compiled into the client configuration; no Client Secret is used or stored.

## Protocol

1. The PS4 sends an HTTPS POST to `https://github.com/login/device/code`
   with `client_id`, `scope=public_repo`, and
   `Accept: application/json`.
2. The UI displays the returned `user_code` and
   `https://github.com/login/device`. The user approves the app on a separate
   browser/device.
3. The PS4 polls `https://github.com/login/oauth/access_token` with
   `client_id`, `device_code`, and
   `grant_type=urn:ietf:params:oauth:grant-type:device_code`, respecting the
   server-provided `interval`. Handle `authorization_pending`,
   `slow_down`, `access_denied`, and `expired_token`.
4. After authorization, upload only user-selected files to the repository
   Contents API over HTTPS. Never upload automatically on capture completion.

## Privacy and token handling

- `public_repo` is requested because the destination repository is public.
  This permits publishing content to public repositories; every committed
  capture/log becomes publicly accessible.
- Require a separate confirmation for each upload batch and show filenames
  and sizes before publishing.
- Provide disconnect/revoke and local credential deletion.
- Never log access/refresh tokens, include them in diagnostics, or commit them.
- Do not persist tokens in plaintext. Until a verified protected-storage
  implementation exists on the target firmware, authorization must remain
  session-only.
- Use TLS certificate validation. Do not disable certificate checks.

## Implementation status

This file and `include/pne/github_oauth.hpp` register the app configuration
and specify the protocol. The PS4 PRX does **not yet** contain the HTTPS
transport, Device Flow polling, token lifecycle, GitHub Contents API uploader,
or upload-confirmation UI. A Client ID alone cannot perform uploads. These
parts must be implemented and tested on PS4 hardware before claiming that
console-to-GitHub upload works.
