# PS4 Neural Engine — NR diagnostics scaffold

This repository is the public home for the PS4 Neural Engine diagnostics and capture integration work.

## Current scope

- Portable telemetry format and file writer.
- Explicit, user-started capture sessions.
- CSV presentation timing logs.
- BMP export only from a CPU-readable RGBA8 staging buffer supplied by a **title-specific, verified adapter**.
- Optional, separately initiated upload of a reviewed diagnostics archive.

## Important hardware boundary

A generic `sceGnmSubmitAndFlipCommandBuffers` hook can observe presentation submissions and timing metadata. It does **not** by itself provide a safe CPU-readable framebuffer. Frame capture requires a per-title adapter that performs a supported GPU readback, waits for GPU completion, handles cache coherency, and validates dimensions and row pitch.

No generic framebuffer interception, GNM compute backend, neural inference runtime, or firmware-14.02-tested PRX is claimed by this repository yet.

## Build target

The PS4 plugin scaffold is intended for OpenOrbis PS4 Toolchain plus GoldHEN_Plugins_SDK. The repository does not redistribute Sony SDK components or proprietary NVIDIA DLLs/weights. Build and hardware validation must be performed with a legally obtained homebrew toolchain and a jailbroken test console.

## Capture controls and privacy

- Capture is OFF by default.
- A user must explicitly start and stop a session on the console.
- Frames and logs remain on the console until the user explicitly exports and uploads them.
- Never embed a GitHub token in a PRX, PKG, source file, or diagnostics archive.
- Review captures for account names, chat, personal information, and copyrighted material before publishing to a public repository.

## Layout

- `src/telemetry.cpp`: portable CSV and BMP writer.
- `include/pne/telemetry.hpp`: capture API.
- `platform/ps4/goldhen/plugin/README.md`: PRX integration contract.
- `docs/UPLOAD.md`: reviewed manual upload procedure.

## Status

This is an integration scaffold, not a finished PS4 PKG or a validated in-game neural renderer. Host-side tests do not prove GNM/PRX compatibility on a PS4 Pro or firmware 14.02.
