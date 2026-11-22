# GoldHEN x64 detour relocation audit — pinned revision

**Date:** 2026-11-22  
**Repository:** `GoldHEN/GoldHEN_Plugins_SDK`  
**Pinned revision:** `bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`  
**Files inspected:** `source/Detour.c`, `source/HDE64.c`, `include/HDE64.h`, `include/Detour.h`, `Makefile`

## Additional finding DETOUR-004 — trampoline copies instructions without relocation

The SDK uses HDE64 to find an instruction-aligned span of at least 14 bytes. It then copies those original bytes verbatim into the trampoline and appends an absolute jump back to the original function.

The pinned HDE64 decoder exposes `F_RELATIVE`, displacement metadata, ModRM fields and instruction length. However, the SDK detour path does not use that metadata to relocate instructions copied into the trampoline.

This matters for at least two x86-64 instruction classes:

- Relative control-flow instructions (short/near conditional branches, calls and jumps) whose displacement is relative to the original instruction address.
- RIP-relative memory operands, commonly encoded with ModRM `mod=00, r/m=101` without address-size override, whose displacement is relative to the original instruction end.

Copying either instruction to a trampoline at a different address without adjusting its displacement changes its target. The current SDK does not reject these instructions or rewrite their displacement.

**Impact:** a trampoline can execute a different control-flow path or access a different address even if allocation and memory protection succeed. This is an additional source-level correctness risk; it is not evidence that this specific GNM function prologue contains such an instruction and is not proof of the reported CE-34878-0 cause.

## What the pinned decoder does and does not provide

- `HDE64.c` decodes instruction length and marks relative immediates with `F_RELATIVE`.
- `HDE64.h` exposes ModRM and displacement fields needed to identify common RIP-relative addressing.
- The decoder is not itself a relocation engine. The SDK's `Detour_DetourFunction64` only copies the decoded span with `memcpy`.
- The inspected SDK Makefile compiles all `source/*.c`, so its HDE64 implementation is part of the pinned SDK library; it is not an external system decoder.

## Revised remediation requirements

A replacement must not blindly copy a decoded prologue. It must either:

1. Relocate every supported relative branch/call and RIP-relative memory operand correctly, with range checks and tests; or
2. Fail closed before touching the target when the stolen span contains an instruction that the implementation cannot safely relocate.

The implementation must also:
- Decode only from a bounded readable window and reject decoder errors/zero or overlong instructions.
- Preserve instruction boundaries and maintain a mapping from original instruction offsets to relocated offsets for branches targeting instructions inside the stolen span.
- Reject branches into the middle of an instruction or unsupported control-flow patterns.
- Prepare trampoline and hook stub fully before target mutation.
- Check every protection transition and verify target bytes after mutation/restoration.
- Keep recovery allocations alive if restoration cannot be proven.
- Define and validate instruction-cache synchronization and concurrent-thread safety before enabling the hook.

## Evidence boundary and decision

This is a pinned-source audit only. It does not establish the actual prologue bytes of `sceGnmSubmitAndFlipCommandBuffers` on firmware 14.02, runtime hook behavior, or the cause of CE-34878-0.

**DETOUR-004: CONFIRMED SOURCE-LEVEL RELOCATION DEFICIENCY.**  
**Target-prologue exposure: NOT YET MEASURED.**  
**PS4 runtime readiness: BLOCKED.**

The implementation plan must now include relocation or a conservative reject policy, in addition to the transactional allocation/protection/restore requirements in `GOLDHEN_DETOUR_SDK_AUDIT_2026-11-22.md`.
