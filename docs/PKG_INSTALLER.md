# PS4 PKG installer

The PKG is a homebrew installer app intended for a jailbroken PS4. Copy the generated `.pkg` to USB or install it through the console's GoldHEN/Debug Package Installer. After installation, launch **PS4 Neural Engine Installer** once; it copies the GoldHEN PRX to `/data/GoldHEN/plugins/ps4_neural_engine.prx` and adds that path to the `[default]` section of `/data/GoldHEN/plugins.ini`. Restart GoldHEN or reboot to load it.

This package currently installs the diagnostics/capture scaffold only. It does not provide a neural inference runtime or a generic game framebuffer adapter. Do not describe it as a completed neural renderer.
