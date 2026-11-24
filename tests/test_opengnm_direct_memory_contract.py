#!/usr/bin/env python3
"""Static audit of the pinned OpenGNM direct-memory and graphics smoke paths."""
import re
import sys
from pathlib import Path

def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"missing function: {signature}")
    opening = source.find("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unterminated function: {signature}")

def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_opengnm_direct_memory_contract.py <opengnm-root>")
    root = Path(sys.argv[1])
    helpers = (root / "src/helpers.c").read_text(encoding="utf-8")
    header = (root / "include/gnm_helpers.h").read_text(encoding="utf-8")
    smoke = (root / "tests/hardware_smoke.c").read_text(encoding="utf-8")

    allocator = function_body(helpers, "GnmError PS4_SYSV_ABI sceGnmDirectMemoryAllocate(")
    assert "sceKernelAllocateDirectMemory(" in allocator
    assert "memory->size, alignment" in allocator
    assert "memorytype, &directmemory" in allocator
    assert "sceKernelMapDirectMemory(" in allocator
    assert "protection, 0, directmemory, alignment" in allocator
    assert "memory->mapped = mapped;" in allocator
    assert "memory->directmemory = (uint64_t)directmemory;" in allocator

    release = function_body(helpers, "void PS4_SYSV_ABI sceGnmDirectMemoryRelease(")
    assert "sceKernelMunmap(memory->mapped, memory->size)" in release
    assert "sceKernelReleaseDirectMemory(" in release
    assert "(int64_t)memory->directmemory, memory->size" in release

    assert re.search(r"void\s*\*\s*mapped;", header)
    assert re.search(r"uint64_t\s+directmemory;", header)

    shader_header = (root / "include/gnm_shader.h").read_text(encoding="utf-8")
    draw_commands = (root / "src/drawcommandbuffer.c").read_text(encoding="utf-8")
    cs_address = function_body(shader_header, "static inline void sceGnmCsRegsSetAddress(")
    assert "(uintptr_t)addr >> 8" in cs_address
    assert "(uintptr_t)addr >> 40" in cs_address
    cs_bind = function_body(draw_commands, "void sceGnmDrawCmdSetCsShaderWithModifier(")
    assert "regs->computepgmhi != 0" in cs_bind
    assert "setshregcompute(cmd, R_00B830_COMPUTE_PGM_LO" in cs_bind
    assert "setshregcompute(cmd, R_00B848_COMPUTE_PGM_RSRC1" in cs_bind
    assert "setshregcompute(cmd, R_00B81C_COMPUTE_NUM_THREAD_X" in cs_bind
    dispatch = function_body(draw_commands, "void sceGnmDrawCmdDispatchDirect(")
    assert "const uint32_t cmddwords = 9;" in dispatch
    assert "PKT3_SHADER_TYPE_S(1)" in dispatch

    production_sources = list((root / "src").glob("*.c"))
    directmemory_uses = []
    for path in production_sources:
        source = path.read_text(encoding="utf-8")
        for match in re.finditer(r"(?:->|\.)directmemory", source):
            directmemory_uses.append((path.name, source.count("\n", 0, match.start()) + 1))
    assert directmemory_uses, "allocator must retain its direct-memory offset"
    assert all(name == "helpers.c" for name, _ in directmemory_uses), (
        f"unexpected production directmemory consumers: {directmemory_uses}"
    )

    allocate = function_body(smoke, "static int allocate_garlic(")
    assert "sceKernelAllocateDirectMemory(" in allocate
    assert "sceKernelMapDirectMemory(" in allocate
    assert "ORBIS_KERNEL_WC_GARLIC" in allocate
    assert "ORBIS_KERNEL_PROT_GPU_READ | ORBIS_KERNEL_PROT_GPU_WRITE" in allocate

    main = function_body(smoke, "int main(void)")
    assert "sceGnmCmdInit(mapped" in main
    assert "(uint64_t)(uintptr_t)eop_label" in main
    assert "sceGnmDrawCmdEventWriteEop(" in main
    assert "sceGnmSubmitCommandBuffers(" in main
    assert "sceGnmSubmitDone()" in main
    assert "for (uint32_t spin = 0; spin < 100000000; ++spin)" in main
    assert "sceGnmDispatchDirect(" not in main
    assert "sceGnmDrawCmdSetCsShader(" not in main

    print("OpenGNM direct-memory and compute-address source contract: PASS")
    print("OpenGNM hardware smoke classification: graphics-only, fixed-iteration polling")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
