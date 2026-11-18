# GoldHEN PRX integration contract

The PRX layer is a platform adapter around the portable telemetry writer.

## Requirements

- Build with OpenOrbis PS4 Toolchain and GoldHEN_Plugins_SDK.
- Use the documented GoldHEN hook API and verify the exact function prototype against the SDK version being built.
- Capture is disabled until the user explicitly starts a session.
- A present hook may record timestamps and buffer indices only.
- A title-specific adapter must own the render-target lifecycle and perform a real GPU-to-CPU readback, fence wait, and cache-coherency handling before passing RGBA8 staging memory to `Writer::write_rgba8_bmp`.

## Do not do this

- Do not dereference a GNM GPU address as a CPU pointer.
- Do not assume the current display buffer is readable from the PRX.
- Do not install a global capture hook for unknown titles and claim universal frame capture.
- Do not upload frames automatically or embed credentials in the plugin.

## Current validation

The PRX source must be compiled against the actual GoldHEN plugin headers and tested on a PS4 Pro before it can be called loadable. A host build of the portable writer does not validate this module.
