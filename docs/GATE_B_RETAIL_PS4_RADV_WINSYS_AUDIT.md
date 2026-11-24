# Gate B — independent retail-PS4 RADV winsys evidence

Date: 2026-11-24

## Purpose and evidence class

This note records a newly identified independent PS4 homebrew graphics stack:
`iHaiDeeZ/Crema` and its shared PS4 RADV/GNM ancestry (`DolphinPS4`,
`ps4-kit`). The public project description says RADV is adapted to submit
work directly to the PS4 AMD GPU. Crema's README explicitly says only the
original PS4 has been tested so far; PS4 Pro reports are requested. This is
therefore useful retail-PS4 implementation evidence, but **not** evidence for
the user's PS4 Pro or firmware 14.02.

Sources:
- Crema README: https://github.com/iHaiDeeZ/Crema
- Crema FAQ: https://github.com/iHaiDeeZ/Crema/blob/main/FAQ.md
- DolphinPS4 PS4 RADV winsys source:
  https://github.com/iHaiDeeZ/DolphinPS4/blob/879b3e57f3d3180bcb5f2234db3903688b28cd7d/port/mesa/ac_ps4_drm.c
- DolphinPS4 Mesa integration patch:
  https://github.com/iHaiDeeZ/DolphinPS4/blob/879b3e57f3d3180bcb5f2234db3903688b28cd7d/patches/mesa-ps4.patch
- Crema PS4 RADV winsys:
  https://github.com/iHaiDeeZ/Crema/blob/475b8d5bd3498dbd5cfefe8175f98d017a017739/port/mesa/ac_ps4_drm.c
- Shared PS4 kit overview: https://github.com/iHaiDeeZ/ps4-kit

## What this independently corroborates

The RADV PS4 winsys source declares and uses the kernel direct-memory API family:
- `sceKernelAllocateDirectMemory`
- `sceKernelMapDirectMemory`
- `sceKernelReleaseDirectMemory`

It also resolves GNM submission entry points by module handle and symbol name,
including `sceGnmSubmitCommandBuffers` and `sceGnmSubmitDone`. This is an
independent implementation pattern for obtaining direct memory and forwarding
command buffers to the GNM driver, rather than relying on the generic OpenGNM
stub submit implementation.

The source search also surfaced a GNM module lookup and symbol-resolution
path in the PS4 winsys. This corroborates that a practical retail homebrew
graphics stack resolves the GNM provider dynamically. It does not establish
the exact module provider selected on PS4 Pro Neo Mode or on firmware 14.02.

The public Crema overview states that rendering runs through Mesa RADV adapted
to submit work directly to the PS4 GPU. Its README says only the original PS4
has been tested so far. The separate xemu-ps4 porting notes report a RADV
startup self-test that performs a graphics draw and readback on PS4; this is
graphics-path evidence only, not compute dispatch evidence:
https://github.com/iHaiDeeZ/xemu-ps4/blob/main/docs/PORTING.md

## What it does not prove for Neural Rendering

This discovery does not close any of the target-specific Gate B requirements:

1. It is not a PS4 Pro test, and does not identify firmware 14.02.
2. The inspected public material does not establish that CPU virtual mappings
   are interchangeable with GPU virtual addresses for NR allocations.
3. It does not prove the residency/registration contract for NR shader code,
   descriptor arrays, or output buffers.
4. GNM submit symbol resolution does not by itself prove the exact FW 14.02
   prototype, argument units, accepted DCB/CCB address form, or provider
   selection between normal and Neo-mode modules.
5. The public source evidence here is principally a Vulkan graphics winsys.
   It does not demonstrate a standalone GNM compute dispatch with known-answer
   output, bounded completion, and full-buffer readback.
6. Graphics readback or a graphics startup self-test cannot substitute for a
   compute write → synchronization → CPU visibility test.

## Design consequences for NR

- Keep CPU mapping, direct-memory allocation offset, and GPU address as
  separately named values. Never silently treat one as another.
- The independent winsys is a source to inspect for allocator and provider
  resolution patterns, not code to copy wholesale into NR.
- Preserve separate normal/Neo provider discovery in the NR ABI census.
- Treat `sceGnmSubmitCommandBuffers` and `sceGnmSubmitDone` as candidate
  firmware entry points until the target provider and exact ABI are verified.
- Require a compute-specific known-answer test; graphics rendering success is
  insufficient.

## Gate B decision

The new evidence strengthens the conclusion that direct-memory allocation,
dynamic GNM symbol resolution, and GNM command submission are used by an
independent PS4 homebrew graphics stack. It narrows the implementation search
space but does not establish NR's firmware 14.02 compute ABI, resource
residency, bounded completion, cache visibility, or deterministic compute
readback on PS4 Pro.

Gate B therefore remains **BLOCKED**. No target-side probe was created or
activated, no compute was submitted, and no PRX, PKG, or console state was
changed as part of this source audit.
