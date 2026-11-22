# PS4 package preflight

The PKG app entry point is `preflight_main.c`. It validates the packaged release manifest against the bundled PRX SHA-256, displays a notification, and exits.

It is deliberately read-only. It does not copy the PRX to `/data/GoldHEN/plugins`, create or edit GoldHEN configuration, load a PRX, or restart GoldHEN. Installing and launching the app therefore does not authorize PRX deployment or activation.

`main.c` is the legacy mutating installer implementation retained for audit/history. It is not linked by `tools/build_pkg.sh`. The package builder must continue to link only `preflight_main.c`, `sha256.c`, and `release_manifest.c` for the app executable.

The PRX remains globally fail-closed because no validated title adapter exists. This package can be used for controlled validation of PKG installation and app startup after build and independent package QA pass; it is not a Neural Rendering runtime release and does not prove PRX ABI or hardware behavior.
