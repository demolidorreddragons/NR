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
