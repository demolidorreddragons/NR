#!/usr/bin/env python3
"""Apply small, auditable fixes to the pinned standalone PSBC checkout."""

from pathlib import Path
import sys


def replace_once(path: Path, old: str, new: str) -> None:
    source = path.read_text(encoding="utf-8")
    count = source.count(old)
    if count != 1:
        raise SystemExit(f"expected exactly one patch anchor in {path}, found {count}")
    path.write_text(source.replace(old, new, 1), encoding="utf-8")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_psbc_gfx8_fixes.py <opengnm-psbc-root>")
    root = Path(sys.argv[1])

    replace_once(
        root / "libpsbc/psbc_compile.c",
        """static void setup_ac_info(struct ac_compiler_info* ac_info, enum amd_gfx_level gfxlevel) {
    ac_info->gfx_level = gfxlevel;
""",
        """static void setup_ac_info(struct ac_compiler_info* ac_info, enum amd_gfx_level gfxlevel) {
    ac_info->gfx_level = gfxlevel;
    /*
     * Match Mesa ac_fill_compiler_info(): GFX7+ workgroups may use 64 KiB
     * of LDS; GFX6 workgroups are limited to 32 KiB. The standalone PSBC
     * target setup previously left this compiler limit zero.
     */
    ac_info->lds_size_per_workgroup = gfxlevel >= GFX7 ? 64 * 1024 : 32 * 1024;
""",
    )

    replace_once(
        root / "cmd/psbc/main.c",
        '#include <stdlib.h>\n\n#include "psbc_compile.h"',
        '#include <stdlib.h>\n#include <stdint.h>\n\n#include "psbc_compile.h"',
    )

    replace_once(
        root / "cmd/psbc/main.c",
        """\tFILE* inputhandle = fopen(opts.inputfile, "r");
\tif (!inputhandle) {
\t\tfatalf("Failed to open input file with %s", strerror(errno));
\t}

\tfseek(inputhandle, 0, SEEK_END);
\tsize_t inputlen = ftell(inputhandle);
\tfseek(inputhandle, 0, SEEK_SET);

\tvoid* input = malloc(inputlen);
\tif (!input) {
\t\tabort();
\t}

\tif (fread(input, 1, inputlen, inputhandle) != inputlen) {
\t\tfatalf("Failed to read input file with %s", strerror(errno));
\t}

\tfclose(inputhandle);
""",
        """\tFILE* inputhandle = fopen(opts.inputfile, "rb");
\tif (!inputhandle) {
\t\tfatalf("Failed to open input file with %s", strerror(errno));
\t}

\tif (fseek(inputhandle, 0, SEEK_END) != 0) {
\t\tfclose(inputhandle);
\t\tfatal("Failed to seek input SPIR-V file");
\t}
\tlong inputlen_long = ftell(inputhandle);
\tif (inputlen_long < 20 || (inputlen_long % 4) != 0) {
\t\tfclose(inputhandle);
\t\tfatal("Invalid SPIR-V file size: truncated header or non-dword length");
\t}
\tsize_t inputlen = (size_t)inputlen_long;
\tif (fseek(inputhandle, 0, SEEK_SET) != 0) {
\t\tfclose(inputhandle);
\t\tfatal("Failed to rewind input SPIR-V file");
\t}

\tvoid* input = malloc(inputlen);
\tif (!input) {
\t\tfclose(inputhandle);
\t\tabort();
\t}

\tif (fread(input, 1, inputlen, inputhandle) != inputlen) {
\t\tfclose(inputhandle);
\t\tfree(input);
\t\tfatalf("Failed to read input file with %s", strerror(errno));
\t}

\tfclose(inputhandle);

\t/* Reject malformed headers before handing untrusted words to NIR/ACO. */
\tconst uint32_t* spirv_header = (const uint32_t*)input;
\tconst uint32_t spirv_version = spirv_header[1];
\tconst uint32_t spirv_major = (spirv_version >> 16) & 0xffu;
\tconst uint32_t spirv_minor = (spirv_version >> 8) & 0xffu;
\tconst uint32_t spirv_revision = spirv_version & 0xffu;
\tif (spirv_header[0] != 0x07230203u || spirv_major != 1u ||
\t    spirv_minor > 6u || spirv_revision != 0u ||
\t    (spirv_version >> 24) != 0u || spirv_header[3] == 0u ||
\t    spirv_header[4] != 0u) {
\t\tfree(input);
\t\tfatal("Invalid SPIR-V header");
\t}
""",
    )

    print("Applied GFX7+ LDS limit and bounded SPIR-V CLI header validation.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
