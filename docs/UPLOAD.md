# Publishing console diagnostics

The console-side capture session stores files locally. Upload is a separate user action.

## Recommended flow

1. Stop the capture session on the PS4.
2. Copy the diagnostics directory to a trusted computer.
3. Inspect CSV and images; remove account names, chat, identifiers, and unrelated content.
4. Create a ZIP containing only the files intended for public release.
5. Use a GitHub token with the minimum required repository Contents permission from a trusted computer.
6. Upload the reviewed archive to a dedicated path such as `diagnostics/session-YYYYMMDD.zip`.

Never place a token in the PS4 plugin, PKG, command-line history, source code, or uploaded archive. Never publish gameplay images without reviewing them first.

## Why upload is not automatic

A public repository makes every committed frame and log accessible to anyone. Explicit review and confirmation are required for each capture session.
