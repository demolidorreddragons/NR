# FW 14.02 libkernel ELF — static audit

## Source and retrieval evidence

- Source path retrieved from the PS4 over the existing read-only FTP endpoint: `/system/common/lib/libkernel.sprx`.
- GitHub Actions run: [45462882389](https://github.com/demolidorreddragons/NR/actions/runs/45462882389).
- Artifact: `ps4-fw1402-libkernel-image` (artifact ID `14953451327`).
- File: `libkernel.sprx.elf`, 449,264 bytes.
- SHA-256: `8a5280aa564b5973cf6b94bb4cc8f41e26aa549e509d4342baa7a68457874f7e`.
- The runner retrieved the file with ordinary FTP RETR. The FTP `SELF` command was not used; its earlier `502` response is bypassed, not repaired.

## ELF structure observed

- ELF64, little-endian, x86-64; OSABI FreeBSD (9).
- `e_type=0xFE18` (Sony dynamic/shared-library ELF).
- 9 program headers; no conventional section-header table.
- `PT_SCE_DYNLIBDATA`: file offset `0x5c330`, size `0x117c0`.
- `PT_DYNAMIC`: file offset `0x6d5b0`, size `0x540`.
- Sony dynamic metadata provides string table offset `0x18` / size `0x5a53`, symbol table offset `0x5a70` / size `0x83e8`, entry size `0x18`.
- The symbol table therefore contains 1,407 entries, of which 1,405 have non-empty encoded NID strings. This is why ordinary `readelf -Ws` did not display the symbols: the image uses Sony's dynamic-data layout rather than standard ELF section headers.

## Relevant symbol entries recovered

The encoded NIDs were matched against public PS4 libkernel NID mappings (including the shadPS4 kernel symbol registrations). Values below are ELF symbol values, not runtime addresses after module loading/ASLR.

| API name | Encoded symbol | ELF value | Size |
|---|---|---:|---:|
| `sceKernelGetModuleList` | `IuxnUuXk6Bg#C#A` | `0x2c7d0` | `0xc1` |
| `sceKernelGetModuleInfo` | `kUpgrXIrz7Q#C#A` | `0x2c940` | `0x98` |
| `sceKernelDlsym` | `LwG8g3niqwA#C#A` | `0x2c710` | `0xb4` |
| `sceKernelGetModuleList2` | `ZzzC3ZGVAkc#T#A` | `0x2d3d0` | `0x73` |
| `sceKernelLoadStartModule` | `wzvqT4UqKX8#C#A` | `0x2bb00` | `0x11` |

All five entries are defined global function symbols in the parsed table (ELF `st_info=0x12`, nonzero section index).

## ABI evidence from static disassembly

The three relevant exported functions were disassembled from the RX `PT_LOAD` segment using their recovered ELF values:

- `sceKernelDlsym` (`0x2c710`) saves incoming `EDI`, `RSI`, and `RDX` and forwards those three values unchanged to its syscall wrapper. This supports the 3-argument shape `(module_handle, symbol_name, address_out)`.
- `sceKernelGetModuleList` (`0x2c7d0`) saves incoming `RDI`, `RSI`, and `RDX`; its wrapper forwards those values unchanged to syscall stubs. No multiplication or conversion of the second argument is performed in this userland export.
- `sceKernelGetModuleInfo` (`0x2c940`) forwards the 32-bit first argument in `EDI` and the second pointer in `RSI` to its syscall wrapper.

The syscall stubs themselves expose the syscall IDs in immediate constants:

- `sceKernelDlsym` calls syscall **591** (`0x24f`).
- `sceKernelGetModuleInfo` calls syscall **593** (`0x251`).
- `sceKernelGetModuleList` performs a runtime version-gate check. Its helper validates an ORBI-tagged version structure (minimum size 0x14) and returns the 32-bit value at offset 0x10. The caller compares that value with `0x05000000`: at or above the threshold it calls syscall **659** (`0x293`); below it calls syscall **592** (`0x250`). For the FW 14.02 target, the newer branch is the applicable one, so the exported `sceKernelGetModuleList` reaches syscall 659 (`sys_dynlib_get_list2`), not the legacy 592 path. The public PS4 syscall reference identifies 659 as `sys_dynlib_get_list2`, while 591/592/593 are the Dlsym/list/info syscalls ([PS4 Developer Wiki — Syscalls](https://www.psdevwiki.com/ps4/Syscalls); [PS4-SCE syscall table](https://github.com/a0zhar/PS4-SCE/blob/main/syscalls.md)).

This materially narrows the FW 14.02 path: the userland export passes `RDI`, `RSI`, and `RDX` unchanged into syscall 659. However, the public syscall table does not provide a confirmed retail prototype or unit for syscall 659's second argument. Do **not** assume that the legacy syscall 592 prototype (`numArray`) proves the semantics of syscall 659. The unit and exact output semantics remain unresolved pending a syscall-659 handler/prototype source specific to the target.

## Limits / next qualification

- The addresses are link-time ELF values; runtime module base/ASLR was not measured.
- NID-to-name matching identifies public API names, but does not establish all retail FW 14.02 structure layouts or kernel-side semantics.
- The current FTP workflow validates ELF64/x86-64 headers and transfers the bytes. It does not invoke or validate the server-side `SELF` extension. The FTP server's `SELF` 502 remains unresolved.
- No console-side code was executed and no console state was changed.
- Next safe static step: trace the referenced syscall numbers and compare against independently sourced FW 14.02 syscall tables/kernel disassembly. Any runtime census still requires a separately qualified ABI and the already-scoped passive-only authorization.


## Cross-check of syscall-659 argument semantics (non-retail evidence)

A follow-up source comparison found two independent emulator implementations for the exported NID `ZzzC3ZGVAkc` / `sceKernelGetModuleList2`:

- [shadPS4 `process.cpp`](https://github.com/shadps4-emu/shadPS4/blob/5c3b6910b99ea4e4d019533490137fe80e400306/src/core/libraries/kernel/process.cpp) declares `sceKernelGetModuleList2(s32* handles, u64 num_array, u64* out_count)`. Its implementation compares the number of output elements against `num_array` and writes one 32-bit handle per element.
- [KytyPS5 `libKernel.cpp`](https://github.com/KytyPS5/KytyPS5/blob/30781dcda029cbd669aa81337ac2902c9c3015d8/src/libs/libKernel.cpp) independently declares `KernelGetModuleList2(int32_t* handles, uint64_t num, uint64_t* count)`, treats `num` as an element capacity, returns `ENOMEM` if the program-ID list exceeds it, copies IDs, and writes the number of elements to `*count`.

These implementations agree on a plausible ABI interpretation for syscall 659's three forwarded arguments: (pointer to 32-bit handle array, **capacity in elements**, pointer to 64-bit output count), rather than a byte-size argument. This is materially better corroboration than inferring semantics from legacy syscall 592.

**Qualification boundary:** both sources are emulator implementations, not Sony kernel source or a FW 14.02 retail syscall handler. They corroborate a candidate contract but do not prove that retail FW 14.02 uses identical validation, error behavior, or exact ABI. Therefore the target census remains blocked pending target-specific qualification; do not silently promote the candidate to a verified retail ABI. The safe next implementation step is to encode this as a documented candidate and keep runtime census fail-closed until the target contract is established.


## Module-info layout cross-check (candidate ABI)

A second independent cross-check now supports the 0x160-byte module-info record layout used by PS4 tooling:

- DolphinPS4's [read-only module sampler](https://github.com/iHaiDeeZ/DolphinPS4/blob/879b3e57f3d3180bcb5f2234db3903688b28cd7d/port/ps4_sampler.cpp) declares `size_t size; char name[256]; ModuleSegment segments[4]; uint32_t segment_count; uint8_t fingerprint[20]`, and calls `sceKernelGetModuleInfo(handle, &info)` after setting `info.size = sizeof(info)`.
- PS4Recomp's [module shim](https://github.com/wie-project/PS4Recomp/blob/97d9f6e3f74717d9fc15ebcf398d9a0808163417/pkg/runtime/modules/libkernel/ps4_kernel_modules.c) uses a size-prefixed module record, fills the name, segment count, and up to four segment records; its comments identify the same public `sceKernelGetModuleInfo` contract.
- KytyPS5's [ModuleInfo2 definition](https://github.com/KytyPS5/KytyPS5/blob/30781dcda029cbd669aa81337ac2902c9c3015d8/src/libs/libKernel.cpp) explicitly asserts `sizeof(ModuleInfo2) == 0x160` and lays out the 256-byte name, four {u64 address, u32 size, s32 prot} segments, segment count, and 20-byte fingerprint.

The arithmetic is consistent on x86-64: 8-byte size + 256-byte name + 4×16-byte segments + 4-byte segment count + 20-byte fingerprint = 0x160 bytes. The name begins at +0x08, segments at +0x108, count at +0x148, and fingerprint at +0x14c.

**Qualification boundary:** this is strong cross-project corroboration of the public PS4 module-info structure, but none of these sources is Sony's FW 14.02 kernel implementation. The 0x160 layout is now a well-supported candidate for passive parsing; it is not a claim that the retail syscall 593 behavior or every field is proven on FW 14.02. Keep the census fail-closed until the target-side ABI can be confirmed safely.

## Direct exported GetModuleList2 wrapper disassembly

A targeted disassembly of the recovered export `sceKernelGetModuleList2` at ELF value `0x2d3d0` provides an additional target-binary cross-check:

- The function saves incoming `RDI`, `RSI`, and `RDX` into callee-saved registers, performs its version/context bookkeeping, then restores those same three values and calls the wrapper at `0x2410`.
- The wrapper at `0x2410` loads `RAX=0x293` (decimal **659**) and executes `syscall`.
- No multiplication, division, scaling, or conversion of the second argument is visible in this exported wrapper. It is passed unchanged to syscall 659.
- This independently confirms from the actual FW 14.02 libkernel image that the explicit `sceKernelGetModuleList2` export reaches syscall 659 with the same three-register forwarding pattern.

This strengthens the target-binary call-path evidence, but it still does not reveal the kernel handler's interpretation of the second argument. In particular, disassembling a userland wrapper cannot establish whether the kernel treats that value as an element count or byte count. The emulator agreement remains candidate corroboration only; retail handler semantics remain unqualified.

## Independent repeat retrieval and byte-for-byte verification (2026-11-24)

A second successful execution of the self-hosted LAN retrieval workflow completed at [run 45464681942](https://github.com/demolidorreddragons/NR/actions/runs/45464681942), producing artifact `ps4-fw1402-libkernel-image` (artifact ID `14954122050`).

The downloaded artifact was independently checked in the analysis environment:

- ZIP SHA-256: `951eaf4d289fdc7929d12eef4f4785c23b075d39a1c5d8ba848c88a9675d8750`.
- Extracted `libkernel.sprx.elf`: 449,264 bytes.
- ELF SHA-256: `8a5280aa564b5973cf6b94bb4cc8f41e26aa549e509d4342baa7a68457874f7e`.
- `cmp` against the previously audited ELF returned identical bytes.
- Header independently rechecked as ELF64, little-endian, FreeBSD ABI, x86-64, Sony OS-specific type `0xFE18`, 9 program headers, and no standard section-header table.

This confirms repeatable retrieval of the same libkernel image through the runner/FTP path. It does not prove SELF-command support, runtime module census, kernel handler ABI, or GPU execution.

## Cross-check of syscall 659 argument contract (external PS4 11.00 evidence; not FW 14.02 qualification)

A targeted source search found a substantially stronger public reference than emulator prototypes alone:

- OpenProspero's PS4 11.00 syscall contract records syscall 659 as `dynlib_get_list2`, with prototype `int dynlib_get_list2(int *handles, size_t max, size_t *count)`. Its evidence pointer is the measured PS4 libkernel stub at `0x2410`; its own status is explicitly `partial`, and the contract notes a console sample where syscall 659 listed one entry while syscall 592 listed 33. Sources: [syscall contract](https://github.com/unemu-org/openprospero/blob/44dc5dc3cea8e528d4e2945c5d100771b47b1b73/contracts/orbis/syscalls.toml) and [kernel implementation](https://github.com/unemu-org/openprospero/blob/44dc5dc3cea8e528d4e2945c5d100771b47b1b73/kernel/sys/compat/sce/sce_dynlib_sys.c).
- The implementation forwards `handles`, `max`, and `count` to its list routine; the prototype's `max` is a `size_t` element capacity, not a byte count. This is useful corroboration for the three-argument ABI family.
- shadPS4 independently declares `sceKernelGetModuleList2(s32* handles, u64 num_array, u64* out_count)`, but this remains emulator behavior and is not used as retail proof: [implementation](https://github.com/shadps4-emu/shadPS4/blob/5c3b6910b99ea4e4d019533490137fe80e400306/src/core/libraries/kernel/process.cpp).

**Impact on this project:** the 14.02 libkernel disassembly already proves that its `sceKernelGetModuleList2` wrapper preserves RDI/RSI/RDX and invokes syscall 659. The new external evidence supports interpreting RSI as an element capacity for the 11.00 contract, but does not establish that FW 14.02's kernel handler is identical or that its module selection matches the required census. Therefore the PR68 implementation remains fail-closed on the existing legacy call; no target-facing call or ABI-dependent code change is justified yet. The remaining qualification requires 14.02 kernel-side evidence or an explicitly authorized, safe retail ABI measurement.


## Important qualification: syscall 593 does not expose the full module record

A review of the PS4 Developer Wiki syscall reference found a limitation that must be applied to the module-info candidate above:

- The reference explicitly states that, since FW 1.76, Sony removed key information from `sys_dynlib_get_info` (syscall 593), including module code/data base addresses and code/data sizes: [PS4 Developer Wiki — Syscalls, changelog and syscall 593](https://www.psdevwiki.com/ps4/Syscalls).
- The same table identifies syscall **660** separately as `sys_dynlib_get_info2`, introduced in the 4.50 generation, but does not publish a prototype or structure contract for it.
- The 0x160 record independently corroborated by DolphinPS4, PS4Recomp, and KytyPS5 is therefore only a candidate structure shape. It must not be interpreted as evidence that syscall 593 on retail 14.02 returns usable segment addresses/sizes, or that its output is interchangeable with syscall 660.

**Impact on passive census:** the current design must not depend on syscall 593 returning segment addresses or sizes. The provider census only needs module identity/name and symbol lookup by name; any future use of segment metadata must first identify the correct retail 14.02 info API and qualify its output. The available FW 14.02 libkernel disassembly currently proves the exported `sceKernelGetModuleInfo` path reaches syscall 593, but does not prove the semantics of syscall 660 or that a usable `sceKernelGetModuleInfo2` export exists in this exact image.

This finding narrows the safe ABI contract and prevents accidentally treating an old/public structure declaration as a full runtime guarantee. It does not qualify the census or authorize a target call.


## Clarification: syscall 593 still returns the module name

The earlier note that syscall 593 has stripped module information needs a precise distinction for the passive provider census:

- The PS4 Developer Wiki says the stripped fields include code/data base addresses and code/data sizes; it does not say the entire record is unusable.
- A separate historical technical note for FW 3.55 reports that the module name remains at offset `+0x08`, while the old code/data fields are zeroed. It identifies syscall 608 (`sys_dynlib_get_info_ex`) as an alternate source for the code base at offset `+0x110`, but does not establish a complete segment-size solution.
- Sources: [PS4 Developer Wiki syscall 593 changelog](https://www.psdevwiki.com/ps4/Syscalls) and [PS4 3.55 notes — System Call 593 and Dumping Modules](https://ps4.editzz.com/3.55/doc/notes.html).

**Impact:** the passive census reads only `info.name`; it does not consume code/data addresses or sizes. The published evidence therefore supports the continued plausibility of name-only enumeration through syscall 593, with the name at `+0x08`. This is historical cross-version evidence, not direct FW 14.02 validation of the full `OrbisKernelModuleInfo` layout or call result. Keep the exact FW 14.02 record-size/output contract marked as a candidate, and do not use syscall 593 for segment metadata.

This clarification narrows the blocker: syscall 593's stripped segment fields do not, by themselves, invalidate name-only provider matching. Remaining retail qualification is the exact 14.02 output record contract and syscall-659 enumeration behavior.

## Critical census-scope caveat: syscall 659 may omit system libraries

A further cross-check changes the risk assessment for the planned GNM provider census:

- The OpenProspero PS4 11.00 syscall-659 contract is explicitly partial and records a console sample in which syscall 659 returned one module while legacy syscall 592 returned 33. This is consistent with a restricted list, not a complete process module inventory.
- shadPS4's HLE implementation of `sceKernelGetModuleList2` explicitly filters out modules marked as system libraries (`IsSystemLib()`), while its `sceKernelGetModuleList` implementation includes the full linker list. Its `sceKernelGetModuleInfo2` also rejects system libraries with `EPERM`. This is emulator behavior, not retail proof, but it is a concrete explanation for the observed restricted-list behavior.
- The actual FW 14.02 `libkernel.sprx` disassembly already established that both the modern `sceKernelGetModuleList` branch and the explicit `sceKernelGetModuleList2` export reach syscall 659. Therefore, simply switching the PR68 caller from one exported name to the other does not bypass the syscall-659 restriction.
- The GNM providers (`libSceGnmDriver` and `libSceGnmDriverForNeoMode`) are system libraries by role. Until retail FW 14.02 evidence proves syscall 659 includes them, the current passive census must be treated as potentially incapable of discovering its target modules.

**Engineering consequence:** do not loosen the fail-closed behavior, do not assume a successful short list is complete, and do not promote the PR68 provider census to target-ready. The next required step is to identify a supported passive enumeration path that includes system libraries, or obtain target-specific evidence that syscall 659 includes the exact GNM provider handles. This is a substantive blocker to the current census approach, not merely an unresolved size-unit detail.

Sources: [OpenProspero PS4 11.00 syscall contract and implementation](https://github.com/unemu-org/openprospero/blob/44dc5dc3cea8e528d4e2945c5d100771b47b1b73/contracts/orbis/syscalls.toml), [shadPS4 process implementation](https://github.com/shadps4-emu/shadPS4/blob/5c3b6910b99ea4e4d019533490137fe80e400306/src/core/libraries/kernel/process.cpp). Both are cross-version/implementation evidence, not a Sony FW 14.02 kernel contract.