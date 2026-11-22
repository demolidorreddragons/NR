# GoldHEN PRX integration contract

The PRX is currently a diagnostics/capture scaffold, not a neural rendering backend.

## Build

- OpenOrbis PS4 Toolchain v0.5.4.
- GoldHEN_Plugins_SDK pinned to `bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`.
- Run `make -C platform/ps4/goldhen/plugin` with `OO_PS4_TOOLCHAIN` and `GOLDHEN_SDK` set.
- GitHub Actions successfully generated a non-empty PRX against the pinned headers; console loading and firmware 14.02 behavior remain unverified.

## Runtime behavior

- Capture is disabled until the user explicitly starts a session.
- Default toggle chord: L3 + L1 + Triangle.
- A present hook records timing and display-buffer metadata only.
- Telemetry writes occur on a worker thread through a bounded queue.
- A title-specific adapter must own render-target access and complete a supported GPU-to-CPU readback, fence wait, and cache-coherency operation before submitting RGBA8 staging memory.

## Safety boundaries

- Never dereference a GNM GPU address as a CPU pointer.
- Never assume the current display buffer is CPU-readable.
- Do not claim universal capture for unknown titles.
- Do not upload frames automatically or embed credentials in the plugin.

## Remaining validation

A successful cross-compile does not establish that GoldHEN can load the PRX on PS4 Pro firmware 14.02. Hardware loading, controller lifecycle, capture file output, and per-title readback adapters still require console validation. No GNM compute backend, neural inference runtime, temporal reconstruction, or generic game framebuffer interception is implemented yet.


## GitHub Device Flow and manual upload controls

- **L3 + L1 + Circle** starts GitHub Device Flow. Enter the displayed one-time code at the verification URL on another device.
- **L3 + L1 + R3 + Circle** disconnects GitHub, cancels any active device authorization, waits for in-flight upload work to finish, clears the in-memory token, and closes the HTTP/TLS session. The authorization chord remains suppressed until all buttons are released.
- The access token is held in process memory only and cleared when the plugin unloads.
- **L3 + L1 + Cross** first previews the batch (count, total size, sample filenames); a second press is required to publish. The uploader publishes `frametimes.csv` and `.bmp` files found in the capture directory, one file per GitHub Contents API commit. Files larger than 16 MiB are skipped.
- Uploads go to the fixed repository path `demolidorreddragons/NR/telemetry/frametimes.csv`. The requested classic `public_repo` scope can write to public repositories available to the authorizing account; it is not repository-restricted.
- HTTPS verification is left enabled. The HTTP/TLS API calls compile against OpenOrbis declarations but still require firmware 14.02 runtime validation. No claim of successful PS4 authorization/upload is made until tested on hardware.

## GNM hook architecture and lifecycle

- The PRX is built for `x86_64-pc-freebsd12-elf`; its GNM hook must use GoldHEN SDK `DetourMode_x64`, not `HOOK32`. In the pinned SDK (`bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`), `HOOK32` selects a 5-byte `E9 rel32` jump and truncates the target displacement to 32 bits without checking the signed ±2 GiB range.
- The project wrapper calls `Detour_DetourFunction` directly, checks the returned stub, cleans a partially allocated trampoline if setup fails, and explicitly unmaps the x64 trampoline after `Detour_Destroy`. The pinned SDK's `Detour_Destroy` only unmaps `StubPtr`, not `TrampolinePtr`.
- This is a source-level/CI correction only. It does not prove `sceKernelMprotect` success, firmware 14.02 hook installation, callback ABI, safe concurrent unload, or runtime stability. Those remain hardware validation gates.
