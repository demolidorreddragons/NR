#!/usr/bin/env python3
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import audit_ps4_self_imports as audit_tool

PT_DYNAMIC = 2
PT_SCE_DYNLIBDATA = 0x61000000


def make_oelf(include_gnm=True):
    strings = bytearray(b"\0")
    kernel_off = len(strings)
    strings.extend(b"libkernel\0")
    gnm_off = len(strings)
    strings.extend(b"libSceGnmDriver\0")
    nid_off = len(strings)
    strings.extend(b"yvZ73uQUqrk#C#C\0")

    segment = bytearray(0x300)
    str_off, sym_off, rela_off, dyn_off = 0x10, 0x80, 0xC0, 0x120
    segment[str_off:str_off + len(strings)] = strings

    # ELF64 symbol zero followed by one undefined Sony NID import.
    struct.pack_into("<IBBHQQ", segment, sym_off + 24,
                     nid_off, 0x10, 0, 0, 0, 0)
    struct.pack_into("<QQq", segment, rela_off, 0xC000, (1 << 32) | 6, 0)

    tags = [
        (0x61000035, str_off),       # DT_SCE_STRTAB
        (0x61000037, len(strings)),  # DT_SCE_STRSZ
        (0x61000039, sym_off),       # DT_SCE_SYMTAB
        (0x6100003F, 48),            # DT_SCE_SYMTABSZ
        (0x6100003B, 24),            # DT_SCE_SYMENT
        (0x6100002F, rela_off),      # DT_SCE_RELA
        (0x61000031, 24),            # DT_SCE_RELASZ
        (0x61000033, 24),            # DT_SCE_RELAENT
        (0x61000015, (1 << 48) | kernel_off),
        (0x6100000F, (1 << 48) | kernel_off),
    ]
    if include_gnm:
        tags.extend([
            (0x61000015, (2 << 48) | gnm_off),
            (0x6100000F, (2 << 48) | gnm_off),
        ])
    tags.append((0, 0))
    for i, pair in enumerate(tags):
        struct.pack_into("<qQ", segment, dyn_off + i * 16, *pair)
    dyn_size = len(tags) * 16

    file_size = 0x200 + len(segment)
    elf = bytearray(file_size)
    elf[:4] = b"\x7fELF"
    elf[4], elf[5] = 2, 1
    struct.pack_into("<H", elf, 18, 62)  # EM_X86_64
    struct.pack_into("<Q", elf, 32, 64)  # e_phoff
    struct.pack_into("<H", elf, 54, 56)
    struct.pack_into("<H", elf, 56, 2)
    struct.pack_into("<IIQQQQQQ", elf, 64,
                     PT_SCE_DYNLIBDATA, 4, 0x200, 0, 0, len(segment), len(segment), 0x1000)
    struct.pack_into("<IIQQQQQQ", elf, 120,
                     PT_DYNAMIC, 4, 0x200 + dyn_off, 0, 0, dyn_size, dyn_size, 8)
    elf[0x200:0x200 + len(segment)] = segment
    return bytes(elf), bytes(segment)


def make_fake_self():
    elf, segment = make_oelf(include_gnm=True)
    segment_file_off = 0x600
    data = bytearray(segment_file_off + len(segment))
    struct.pack_into("<I", data, 0, audit_tool.SELF_MAGIC)
    struct.pack_into("<H", data, 0x18, 2)
    # Segment id is encoded in bits 20..27 and must match PT_SCE_DYNLIBDATA phdr index 0.
    struct.pack_into("<QQQQ", data, 0x20, 0x800, segment_file_off, len(segment), len(segment))
    struct.pack_into("<QQQQ", data, 0x40, (1 << 20) | 0x800, 0, 0, 0)
    elf_base = 0x20 + 2 * 0x20
    data[elf_base:elf_base + 0x120] = elf[:0x120]
    data[segment_file_off:segment_file_off + len(segment)] = segment
    return bytes(data)


class SonyImportAuditTests(unittest.TestCase):
    def test_raw_oelf_requires_relocated_gnm_nid(self):
        elf, _ = make_oelf()
        _off, _size, libs, modules, nids = audit_tool.audit(elf)
        self.assertEqual(
            audit_tool.verify(
                libs, modules, nids,
                ["libSceGnmDriver:libSceGnmDriver:yvZ73uQUqrk"], []
            ),
            [],
        )

    def test_raw_oelf_rejects_forbidden_gnm_library(self):
        elf, _ = make_oelf(include_gnm=False)
        _off, _size, libs, modules, nids = audit_tool.audit(elf)
        self.assertEqual(audit_tool.verify(libs, modules, nids, [], ["libSceGnmDriver"]), [])

    def test_wrong_nid_fails(self):
        elf, _ = make_oelf()
        _off, _size, libs, modules, nids = audit_tool.audit(elf)
        errors = audit_tool.verify(
            libs, modules, nids,
            ["libSceGnmDriver:libSceGnmDriver:wrongNidValue"], []
        )
        self.assertTrue(any("required relocated import missing" in e for e in errors))

    def test_fake_self_extracts_and_audits_same_import(self):
        fake_self = make_fake_self()
        _off, _size, libs, modules, nids = audit_tool.audit(fake_self)
        self.assertEqual(
            audit_tool.verify(
                libs, modules, nids,
                ["libSceGnmDriver:libSceGnmDriver:yvZ73uQUqrk"], []
            ),
            [],
        )


if __name__ == "__main__":
    unittest.main()
