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

All five entries are defined global function symbols in the parsed table (ELF `st_info=0x12`, nonzero section index). This is concrete static evidence that the retrieved image contains the module-enumeration, module-info and symbol-lookup entry points needed for the passive provider census.

## Limits / next qualification

- The addresses are link-time ELF values; runtime module base/ASLR was not measured.
- NID-to-name matching identifies public API names, but does not establish the retail FW 14.02 prototypes, structure sizes, or `sceKernelGetModuleList` size-argument unit.
- The current FTP workflow validates ELF64/x86-64 headers and transfers the bytes. It does not invoke or validate the server-side `SELF` extension. The FTP server's `SELF` 502 remains unresolved.
- No console-side code was executed and no console state was changed.
- Next safe static step: disassemble these recovered functions and compare their argument/register use and referenced structures against the existing FW 14.02 ABI hypotheses. Any runtime census still requires a separately qualified ABI and the already-scoped passive-only authorization.
