# GoldHEN Plugin Source package

This is a **PRX-only** archive for GoldHEN's local Plugin Source updater. It is
not the official GoldHEN GitHub plugin collection.

## Build output

CI creates:

- `PS4-Neural-Engine-GoldHEN.zip`
- `PS4-Neural-Engine-GoldHEN.zip.sha256`

The ZIP contains exactly one file, `ps4_neural_engine.prx`, at the archive
root. GoldHEN's local archive updater is expected to extract plugin binaries
into `/data/GoldHEN/plugins/`.

The archive deliberately does **not** contain `plugins.ini`: replacing that
file could disable or remove the user's existing plugin configuration.

## Install using the GoldHEN menu

1. Download the ZIP from the GitHub Actions artifact named
   `ps4-neural-engine-build`.
2. For USB source, copy it to `UPDATE/plugins.zip` on the USB drive.
   For HDD source, copy it to
   `/data/GoldHEN/UPDATE/plugins.zip`.
3. In GoldHEN, choose the matching local Plugin Source (USB or HDD) and run
   **Update Plugin Archive**.
4. Confirm through FTP that
   `/data/GoldHEN/plugins/ps4_neural_engine.prx` exists.

## Enable the plugin

GoldHEN's Plugin Loader reads `/data/GoldHEN/plugins.ini`. Preserve any
existing sections and add this line under `[default]`:

```ini
/data/GoldHEN/plugins/ps4_neural_engine.prx
```

If `[default]` does not exist, create it before the line. Do not replace the
whole file if it already contains other plugin entries. Restart GoldHEN or
reboot after updating the configuration.

## Important compatibility boundary

A successful archive build and extraction do not prove that this PRX loads on
the console. Firmware 14.02 / GoldHEN 2.4b18.14 runtime loading still needs to
be tested. The current PRX is a diagnostics/capture scaffold, not a completed
neural renderer. Capture remains off until explicitly started.
