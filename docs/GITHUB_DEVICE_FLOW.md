# GitHub authorization for the PS4

The registered OAuth App uses GitHub Device Flow. The public Client ID is
compiled into the client configuration; no Client Secret is used or stored.

- Client ID configured: `0v23liEuX7IS6L79I8A6`.
- The OAuth App registration's Redirect URL is metadata required by GitHub's
  registration form; Device Flow does not use a browser callback or redirect
  on the PS4. The console displays the verification URL and user code instead.

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
4. After authorization, the PS4 previews the pending telemetry batch. A second
   explicit controller confirmation publishes that batch through the Contents API.
   Nothing is uploaded automatically when a capture completes.

## Privacy and token handling

- `public_repo` is requested because the destination repository is public.
  This is a classic OAuth scope and is broader than this one repository: it can
  permit writes to other public repositories accessible to the authorizing
  account. The client must hard-code the destination `demolidorreddragons/NR`
  and must never accept an arbitrary repository/path from runtime input.
  Every committed capture/log becomes publicly accessible.
- Require a two-press confirmation for each batch. The first press previews
  the file count, total size, and sample filenames; the second press publishes.
- Local disconnect clears the session token; remote OAuth grant revocation is performed in GitHub Settings → Applications.
- Never log access/refresh tokens, include them in diagnostics, or commit them.
- Do not persist tokens in plaintext. Until a verified protected-storage
  implementation exists on the target firmware, authorization must remain
  session-only.
- Use TLS certificate validation. Do not disable certificate checks.

## Implementation status

The PRX now contains an OpenOrbis `sceHttp`/`sceSsl` HTTPS client, Device Flow
polling, in-memory-only access-token handling, cancellation on plugin unload,
and a GitHub Contents API uploader. Controller actions:

- L3 + L1 + Circle: begin Device Flow; authorize using the displayed code.
- L3 + L1 + Cross: first press previews the batch; second press confirms upload.
- L3 + L1 + R3 + Circle: clear the local session token and cancel any active authorization. This does not revoke the OAuth grant at GitHub; revoke it separately in GitHub Settings → Applications.
- The batch includes `frametimes.csv` and BMP files already present in the capture
  directory; each file is committed separately. Files above 16 MiB are skipped.

The uploader targets only the hard-coded repository and paths. TLS verification
is not disabled. Host CI and PS4 cross-build must pass, and the HTTP/Device Flow
behavior still requires runtime validation on a PS4 Pro with firmware 14.02 and
GoldHEN. A successful cross-build is not proof of live GitHub authentication or
upload. Capture images also remain dependent on a game-specific adapter that
provides valid CPU-readable RGBA8 staging data.
