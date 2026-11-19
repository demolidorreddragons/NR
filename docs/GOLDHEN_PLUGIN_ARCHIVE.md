# GoldHEN Plugin Source package

This is a **PRX-only** archive for GoldHEN's local Plugin Source updater. It
is not a self-installing package: the GoldHEN archive updater extracts files,
but does not execute the Neural Engine installer or merge entries into
`plugins.ini`.

## Automatic installation (recommended)

Use the **PS4 Neural Engine Installer PKG** generated in the same CI artifact.
Install and launch that PKG from the PS4 Package Installer. Its installer
copies the PRX into `/data/GoldHEN/plugins/`, safely adds the path under
`[default]` in `/data/GoldHEN/plugins.ini`, preserves existing entries, and
writes a status log to
`/data/GoldHEN/ps4_neural_engine_install.log`. Restart GoldHEN or reboot to
attempt plugin loading.

If installation fails, the log records the exact failing stage and errno.
This is diagnostic information; a successful file/config installation does
not by itself prove that the PRX is compatible with the running GoldHEN build.

## Manual archive-only deployment

CI also creates `PS4-Neural-Engine-GoldHEN.zip`. It contains exactly one file,
`ps4_neural_engine.prx`, at the archive root. This archive is useful only for
placing the PRX through GoldHEN's local Plugin Source updater; it cannot
automatically configure `plugins.ini`.

- USB source: copy the ZIP to `UPDATE/plugins.zip` on the USB drive.
- HDD source: copy it to `/data/GoldHEN/UPDATE/plugins.zip`.
- Select the matching local Plugin Source (USB or HDD) and run
  **Update Plugin Archive**.

Do not replace `plugins.ini` with a packaged copy: doing so could remove the
user's existing plugin configuration. For automatic setup, use the Installer
PKG described above.

## Compatibility boundary

A successful build or file/config installation does not prove that this PRX
loads on the console. Firmware 14.02 / GoldHEN 2.4b18.14 runtime loading still
needs to be tested. The current PRX is a diagnostics/capture scaffold, not a
completed neural renderer. Capture remains off until explicitly started.
