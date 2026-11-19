# PS4 PKG installer

The PKG is a homebrew installer app. After installation, launch **PS4 Neural Engine Installer** once; it copies the GoldHEN PRX to `/data/GoldHEN/plugins/ps4_neural_engine.prx` and adds that path to the `[default]` section of `/data/GoldHEN/plugins.ini`. Restart GoldHEN or reboot to load it.

This package currently installs the diagnostics/capture scaffold only. It does not provide a neural inference runtime or a generic game framebuffer adapter. Do not describe it as a completed neural renderer.
