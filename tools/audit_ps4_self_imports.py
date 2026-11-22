#!/usr/bin/env python3
"""Audit Sony import metadata in a PS4 OELF or unencrypted fake SELF/PRX.

Supports raw OELF and OpenOrbis create-fself fake SELF output. Unsupported
encrypted/compressed SELF segments fail closed; no offsets are guessed.
"""
from __future__ import annotations
import argparse
import struct
import sys
from pathlib import Path

ELF_MAGIC = b"\x7fELF"
SELF_MAGIC = 0x1D3D154F
PT_DYNAMIC = 2
PT_SCE_DYNLIBDATA = 0x61000000
DT_NULL = 0
DT_SCE_NEEDED_MODULE = 0x6100000F
DT_SCE_IMPORT_LIB = 0x61000015
DT_SCE_STRTAB = 0x61000035
DT_SCE_STRSZ = 0x61000037
DT_SCE_SYMTAB = 0x61000039
DT_SCE_SYMENT = 0x6100003B
DT_SCE_SYMTABSZ = 0x6100003F
DT_SCE_RELA = 0x6100002F
DT_SCE_RELASZ = 0x61000031
DT_SCE_RELAENT = 0x61000033
DT_SCE_JMPREL = 0x61000029
DT_SCE_PLTRELSZ = 0x6100002D


def u16(data, off):
    return struct.unpack_from("<H", data, off)[0]


def u32(data, off):
    return struct.unpack_from("<I", data, off)[0]


def u64(data, off):
    return struct.unpack_from("<Q", data, off)[0]


def program_headers(elf, base):
    if elf[base:base + 4] != ELF_MAGIC:
        raise ValueError("embedded ELF magic missing")
    if elf[base + 4] != 2 or elf[base + 5] != 1 or u16(elf, base + 18) != 62:
        raise ValueError("only little-endian x86-64 ELF is supported")
    phoff, entsize, count = u64(elf, base + 32), u16(elf, base + 54), u16(elf, base + 56)
    if entsize != 56 or not 0 < count <= 128:
        raise ValueError(f"invalid ELF program header layout: {entsize}/{count}")
    out = []
    for i in range(count):
        pos = base + phoff + i * entsize
        if pos + entsize > len(elf):
            raise ValueError("program header table exceeds file")
        typ, _flags, off, _va, _pa, size, _memsz, _align = struct.unpack_from("<IIQQQQQQ", elf, pos)
        out.append((i, typ, off, size))
    return out


def extract_dynlib(data):
    if data.startswith(ELF_MAGIC):
        base = 0
        ph = program_headers(data, base)
        dynlib = next((x for x in ph if x[1] == PT_SCE_DYNLIBDATA), None)
        dynamic = next((x for x in ph if x[1] == PT_DYNAMIC), None)
        if not dynlib or not dynamic:
            raise ValueError("ELF lacks PT_SCE_DYNLIBDATA/PT_DYNAMIC")
        _, _, doff, dsize = dynlib
        _, _, toff, tsize = dynamic
        if doff + dsize > len(data) or not (doff <= toff <= toff + tsize <= doff + dsize):
            raise ValueError("invalid dynamic/dynlib segment bounds")
        return data[doff:doff + dsize], toff - doff, tsize, doff

    if len(data) < 0x20 or u32(data, 0) != SELF_MAGIC:
        raise ValueError("input is neither OELF nor supported fake SELF")
    count = u16(data, 0x18)
    if not 0 < count <= 128:
        raise ValueError(f"invalid SELF segment count {count}")
    base = 0x20 + count * 0x20
    ph = program_headers(data, base)
    dynlib = next((x for x in ph if x[1] == PT_SCE_DYNLIBDATA), None)
    dynamic = next((x for x in ph if x[1] == PT_DYNAMIC), None)
    if not dynlib or not dynamic:
        raise ValueError("SELF ELF lacks PT_SCE_DYNLIBDATA/PT_DYNAMIC")
    ph_index, _, doff, dsize = dynlib
    _, _, toff, tsize = dynamic
    if not (doff <= toff <= toff + tsize <= doff + dsize):
        raise ValueError("PT_DYNAMIC is not contained in PT_SCE_DYNLIBDATA")
    candidates = []
    for i in range(count):
        flags, fileoff, packed, unpacked = struct.unpack_from("<QQQQ", data, 0x20 + i * 0x20)
        seg_id = (flags >> 20) & 0xFF
        if seg_id != ph_index or packed != dsize:
            continue
        if flags & 0x2:
            raise ValueError("encrypted SELF segment unsupported")
        if flags & 0x8 or packed != unpacked:
            raise ValueError("compressed SELF segment unsupported")
        if fileoff + packed > len(data):
            raise ValueError("SELF segment exceeds file")
        candidates.append(data[fileoff:fileoff + packed])
    if len(candidates) != 1:
        raise ValueError(f"expected one raw dynlib SELF segment, found {len(candidates)}")
    return candidates[0], toff - doff, tsize, doff


def audit(data):
    segment, dynoff, dynsize, dynfileoff = extract_dynlib(data)
    tags = []
    if dynoff < 0 or dynoff + dynsize > len(segment):
        raise ValueError("dynamic table bounds invalid")
    for off in range(dynoff, dynoff + dynsize, 16):
        tag, value = struct.unpack_from("<qQ", segment, off)
        if tag == DT_NULL:
            break
        tags.append((tag, value))
    else:
        raise ValueError("dynamic table missing DT_NULL")

    def one(tag, required=True):
        vals = [v for t, v in tags if t == tag]
        if not vals and required:
            raise ValueError(f"required dynamic tag {tag:#x} missing")
        if len(vals) > 1:
            raise ValueError(f"dynamic tag {tag:#x} unexpectedly repeated")
        return vals[0] if vals else None

    stroff, strsz = one(DT_SCE_STRTAB), one(DT_SCE_STRSZ)
    symoff, symsz, syment = one(DT_SCE_SYMTAB), one(DT_SCE_SYMTABSZ), one(DT_SCE_SYMENT)
    if syment != 24 or symsz % syment or stroff + strsz > len(segment) or symoff + symsz > len(segment):
        raise ValueError("invalid Sony string/symbol table")
    strings = segment[stroff:stroff + strsz]

    def cstr(off):
        if off < 0 or off >= len(strings):
            raise ValueError(f"string offset {off:#x} out of range")
        end = strings.find(b"\0", off)
        if end < 0:
            raise ValueError("unterminated Sony string")
        return strings[off:end].decode("ascii", "strict")

    libraries = []
    for _, value in (x for x in tags if x[0] == DT_SCE_IMPORT_LIB):
        libraries.append(((value >> 48) & 0xFFFF, cstr(value & 0xFFFFFFFF)))
    modules = []
    for _, value in (x for x in tags if x[0] == DT_SCE_NEEDED_MODULE):
        modules.append(((value >> 48) & 0xFFFF, cstr(value & 0xFFFFFFFF)))

    relocs = {}
    relaoff, relasz = one(DT_SCE_RELA, False), one(DT_SCE_RELASZ, False)
    relaent = one(DT_SCE_RELAENT, False) or 24
    if relaoff is not None and relasz is not None:
        if relaent != 24 or relasz % relaent or relaoff + relasz > len(segment):
            raise ValueError("invalid DT_SCE_RELA bounds")
        for off in range(relaoff, relaoff + relasz, relaent):
            ro, ri, _add = struct.unpack_from("<QQq", segment, off)
            relocs.setdefault(ri >> 32, []).append(ro)
    jmpoff, jmpsz = one(DT_SCE_JMPREL, False), one(DT_SCE_PLTRELSZ, False)
    if jmpoff is not None and jmpsz is not None:
        if jmpsz % 24 or jmpoff + jmpsz > len(segment):
            raise ValueError("invalid DT_SCE_JMPREL bounds")
        for off in range(jmpoff, jmpoff + jmpsz, 24):
            ro, ri, _add = struct.unpack_from("<QQq", segment, off)
            relocs.setdefault(ri >> 32, []).append(ro)

    nids = []
    for i in range(symsz // syment):
        nameoff, _info, _other, shndx, _value, _size = struct.unpack_from("<IBBHQQ", segment, symoff + i * syment)
        if shndx != 0 or nameoff == 0:
            continue
        parts = cstr(nameoff).split("#")
        if len(parts) == 3 and len(parts[0]) == 11 and len(parts[1]) == len(parts[2]) == 1:
            nids.append((i, parts[0], parts[1], parts[2], tuple(relocs.get(i, ()))))
    return dynfileoff, len(segment), libraries, modules, nids


def verify(libraries, modules, nids, required, forbidden):
    errors = []
    libmap, modmap = dict(libraries), dict(modules)
    names = {n for _, n in libraries} | {n for _, n in modules}
    for name in forbidden:
        if name in names:
            errors.append(f"forbidden library/module present: {name}")
    for spec in required:
        try:
            lib, mod, nid = spec.split(":", 2)
        except ValueError:
            errors.append(f"invalid requirement {spec!r}; expected LIB:MODULE:NID")
            continue
        found = False
        for _idx, value, libletter, modletter, reloc in nids:
            if value != nid or not reloc:
                continue
            li, mi = ord(libletter) - 65, ord(modletter) - 65
            if libmap.get(li) == lib and modmap.get(mi) == mod:
                found = True
                break
        if not found:
            errors.append(f"required relocated import missing: {lib}:{mod}:{nid}")
    return errors


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--input", required=True, help="PS4 OELF or fake SELF/PRX")
    ap.add_argument("--require-import", action="append", default=[], metavar="LIB:MODULE:NID")
    ap.add_argument("--forbid-library", action="append", default=[])
    args = ap.parse_args()
    try:
        off, size, libs, mods, nids = audit(Path(args.input).read_bytes())
        errors = verify(libs, mods, nids, args.require_import, args.forbid_library)
        print(f"PT_SCE_DYNLIBDATA_OFFSET={off:#x}")
        print(f"PT_SCE_DYNLIBDATA_SIZE={size:#x}")
        print("SONY_IMPORT_LIBRARIES=" + ";".join(f"{i}:{n}" for i, n in libs))
        print("SONY_NEEDED_MODULES=" + ";".join(f"{i}:{n}" for i, n in mods))
        for idx, nid, libletter, modletter, relocs in nids:
            if relocs and (nid in [r.split(":", 2)[2] for r in args.require_import]):
                print(f"SONY_NID_IMPORT={nid}#{libletter}#{modletter};SYMBOL_INDEX={idx};RELOCATIONS=" + ",".join(hex(x) for x in relocs))
        if errors:
            for e in errors:
                print("SONY_IMPORT_AUDIT_ERROR=" + e, file=sys.stderr)
            print("SONY_IMPORT_AUDIT=FAIL")
            return 1
        print("SONY_IMPORT_AUDIT=PASS")
        return 0
    except Exception as e:
        print(f"SONY_IMPORT_AUDIT_ERROR={e}", file=sys.stderr)
        print("SONY_IMPORT_AUDIT=FAIL")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
