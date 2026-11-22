# GoldHEN PRX integration contract

The PRX is currently a diagnostics/capture scaffold, not a neural rendering backend.

## Build

- OpenOrbis PS4 Toolchain v0.5.4.
- GoldHEN_Plugins_SDK pinned to `bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`.
- Run `make -C platform/ps4/goldhen/plugin` with `OO_PS4_TOOLCHAIN` and `GOLDHEN_SDK` set.
- GitHub Actions successfully generated a non-empty PRX against the pinned headers; console loading and firmware 14.02 behavior remain unverified.

## Runtime behavior

- The plugin is registered in GoldHEN's `[default]` section, but **the PRX is now globally inert**: `has_validated_title_adapter()` returns false and `plugin_load` exits before frequency setup, GNM hook installation, or worker creation. This is intentional because no title-owned cooperative rendezvous adapter, mapping-lifetime contract, or per-consumer code-publication handoff is integrated. The previous own-title-ID guard remains an additional exclusion, not the safety mechanism. Do not enable the hook by changing this predicate alone; first integrate and validate the complete title-specific contract.

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
- NR now builds `hde64_bounded.c` and `hde64_bounded_plan.c` into the PRX as separate, currently unused modules. The decoder adapter wraps the pinned SDK decoder using a 32-byte zero-padded scratch window, copies at most 15 bytes from a caller-declared readable span, and rejects incomplete instructions and all upstream decoder errors. The span planner builds an instruction-aligned preflight plan up to 64 bytes/16 instructions and rejects all relative-control-flow, unsupported-relative, and RIP-relative instructions before any target mutation.
- The adapter exposes normalized relative-immediate and displacement offsets/sizes, CALL/JMP/Jcc/LOOP classifications, ModRM/SIB and address-size-override metadata, and a conservative RIP-relative indicator. Relative encodings not recognized by the classifier are marked unsupported; consumers must fail closed. The pinned upstream decoder mishandles the address-size-override ModRM `mod=00,r/m=101` disp32 form, so this adapter explicitly rejects that encoding rather than accepting an incomplete decode.
- **The readable-span precondition is strict:** the caller must already know that every byte it declares is readable and stable for the duration of the copy. The adapter cannot validate arbitrary pointers, prevent concurrent unmapping, or solve query/read TOCTOU. It is not a relocator and is not called by the live GNM hook.
- Host CI tests exercise guard-page truncation rejection, null/empty/exact-span boundaries, a 15-byte maximum-length instruction, relative/RIP/displacement metadata, and the fail-closed stolen-span planner (including maximum byte/instruction limits). An AddressSanitizer sweep also tested 150,000 random spans against a guard page with no out-of-span read. Host suite passed in run `45056416166`; PRX build and independent PKG QA both passed in run `45056164974` (`PASS_PC_ONLY_NOT_HARDWARE_TESTED`). Firmware 14.02 ABI/runtime behavior, relocation, transactional protection changes, cache publication, and callback quiescence remain separate gates.
- This source-level/CI work does not prove `sceKernelMprotect` success, firmware 14.02 hook installation, callback ABI, safe concurrent unload, or runtime stability. Those remain hardware validation gates.

- A fail-closed detour transaction state-machine design is documented in `docs/audits/NR_DETOUR_TRANSACTION_STATE_MACHINE_DESIGN_2026-11-22.md`. It adds an explicit install-time execution-rendezvous gate: a 14-byte target patch is not atomic, so teardown refcounting alone cannot make installation safe. This is a design artifact only; no transaction core or PS4 rendezvous adapter is implemented, and the live hook remains on the upstream SDK path.

- `detour_transaction_core.cpp` is a **host-only** transaction coordinator with injected operations and explicit `COMPLETE` / `NO_EFFECT` / `UNKNOWN` outcomes. It exercises preparation-before-mutation, execution-window gating, patch verification, rollback after uncertain writes, and poisoned resource retention. Its removal path requires a separate callback that blocks new entries and drains in-flight target/hook/stub/trampoline execution before restoring original bytes and releasing resources. It is registered only in the root host CMake test target and is deliberately excluded from the PS4 PRX Makefile. Its callbacks are test contracts, not implementations of PS4 memory, cache, or thread-rendezvous primitives; the live hook remains unchanged.


- A follow-up audit found `scePthreadSuspendAll()` / `scePthreadResumeAll()` in the exact OpenOrbis v0.5.4 header, both declared `void` with no documented semantics. FreeBSD's similarly named `pthread_suspend_all_np()` suspends all active threads except the caller and warns against using non-async-signal-safe functions until resume, but this is not proof of Orbis/firmware 14.02 behavior. The PS4 symbols are therefore a research lead only: they are **not** approved as an install rendezvous or teardown-quiescence primitive and are not called by NR. See `docs/audits/PS4_PTHREAD_SUSPEND_RENDEZVOUS_AUDIT_2026-11-22.md`.


- A mapping/code-publication follow-up audit confirms two additional hard blockers. The public `sceKernelVirtualQuery` / `sceKernelQueryMemoryProtection` declarations expose metadata queries, not a mapping pin/lease; query-then-read does not prove the title's executable mapping remains stable. Separately, AMD64 cross-modifying-code guidance requires a synchronous consumer handoff and a serializing instruction on each receiving thread before it enters the modified stream; no evidence shows that Orbis `scePthreadResumeAll` supplies this. The 14-byte patch does not meet AMD's constrained asynchronous single-quadword method. Thus neither query APIs nor a guessed cache/barrier call nor suspend/resume alone qualifies as a safe adapter. See `docs/audits/PS4_MAPPING_STABILITY_AND_CODE_PUBLICATION_AUDIT_2026-11-22.md`.


- **Fail-closed disposition (2026-11-22):** the live startup path is gated by has_validated_title_adapter(), which currently returns false. The legacy x64 detour implementation remains in source for audit/reference but is unreachable from plugin_load. The plugin performs no GNM interception, controller startup, capture, or upload while this gate is false. Host source-contract CI asserts the false gate and that it precedes both hook and worker startup. This is not a hardware-tested change.
