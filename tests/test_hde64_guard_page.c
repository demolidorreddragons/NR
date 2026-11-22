#define _GNU_SOURCE
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "hde64_bounded.h"

static int raw_decoder_faults(const uint8_t *bytes, size_t length, const char *name) {
    long page_size = sysconf(_SC_PAGESIZE);
    uint8_t *region = mmap(NULL, (size_t)page_size * 2, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) return 0;
    if (mprotect(region + page_size, (size_t)page_size, PROT_NONE) != 0) {
        munmap(region, (size_t)page_size * 2);
        return 0;
    }
    uint8_t *start = region + page_size - length;
    memcpy(start, bytes, length);
    pid_t child = fork();
    if (child == 0) {
        hde64s decoded;
        (void)hde64_disasm(start, &decoded);
        _exit(0);
    }
    int status = 0;
    int ok = child > 0 && waitpid(child, &status, 0) == child &&
             WIFSIGNALED(status) &&
             (WTERMSIG(status) == SIGSEGV || WTERMSIG(status) == SIGBUS);
    munmap(region, (size_t)page_size * 2);
    if (!ok) fprintf(stderr, "FAIL: raw decoder did not fault on truncated %s (status=%d)\n", name, status);
    return ok;
}

static int bounded_rejects(const uint8_t *bytes, size_t length, const char *name) {
    long page_size = sysconf(_SC_PAGESIZE);
    uint8_t *region = mmap(NULL, (size_t)page_size * 2, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) return 0;
    if (mprotect(region + page_size, (size_t)page_size, PROT_NONE) != 0) {
        munmap(region, (size_t)page_size * 2);
        return 0;
    }
    uint8_t *start = region + page_size - length;
    memcpy(start, bytes, length);
    nr_hde64_decoded decoded;
    int ok = !nr_hde64_decode_bounded(start, length, &decoded);
    munmap(region, (size_t)page_size * 2);
    if (!ok) fprintf(stderr, "FAIL: bounded decoder accepted truncated %s\n", name);
    return ok;
}

static int expect_relative(const uint8_t *bytes, size_t size,
                           nr_hde64_relative_kind kind,
                           uint8_t offset, uint8_t width, const char *name) {
    nr_hde64_decoded decoded;
    if (!nr_hde64_decode_bounded(bytes, size, &decoded) ||
        decoded.relative_kind != kind || decoded.relative_offset != offset ||
        decoded.relative_size != width) {
        fprintf(stderr, "FAIL: relative metadata for %s\n", name);
        return 0;
    }
    return 1;
}

static int test_metadata(void) {
    const uint8_t call_rel32[] = {0xe8, 0, 0, 0, 0};
    const uint8_t jmp_rel8[] = {0xeb, 0};
    const uint8_t jcc_rel8[] = {0x75, 0};
    const uint8_t jcc_rel32[] = {0x0f, 0x85, 0, 0, 0, 0};
    const uint8_t loop_rel8[] = {0xe2, 0};
    const uint8_t rip_load[] = {0x48, 0x8b, 0x05, 0, 0, 0, 0};
    const uint8_t rip_store_imm[] = {0xc7, 0x05, 0, 0, 0, 0, 1, 0, 0, 0};
    const uint8_t sib_absolute[] = {0x48, 0x8b, 0x04, 0x25, 0x78, 0x56, 0x34, 0x12};
    const uint8_t addr_override[] = {0x67, 0x48, 0x8b, 0x05, 0, 0, 0, 0};
    const uint8_t rbp_disp8[] = {0x48, 0x8b, 0x45, 0xf8};
    nr_hde64_decoded decoded;

    if (!expect_relative(call_rel32, sizeof(call_rel32), NR_HDE64_RELATIVE_CALL, 1, 4, "CALL rel32") ||
        !expect_relative(jmp_rel8, sizeof(jmp_rel8), NR_HDE64_RELATIVE_JUMP, 1, 1, "JMP rel8") ||
        !expect_relative(jcc_rel8, sizeof(jcc_rel8), NR_HDE64_RELATIVE_CONDITIONAL, 1, 1, "Jcc rel8") ||
        !expect_relative(jcc_rel32, sizeof(jcc_rel32), NR_HDE64_RELATIVE_CONDITIONAL, 2, 4, "Jcc rel32") ||
        !expect_relative(loop_rel8, sizeof(loop_rel8), NR_HDE64_RELATIVE_LOOP, 1, 1, "LOOP rel8"))
        return 0;

    if (!nr_hde64_decode_bounded(rip_load, sizeof(rip_load), &decoded) ||
        !decoded.rip_relative || decoded.displacement_offset != 3 ||
        decoded.displacement_size != 4) {
        fprintf(stderr, "FAIL: RIP-relative MOV metadata\n");
        return 0;
    }
    if (!nr_hde64_decode_bounded(rip_store_imm, sizeof(rip_store_imm), &decoded) ||
        !decoded.rip_relative || decoded.displacement_offset != 2 ||
        decoded.displacement_size != 4) {
        fprintf(stderr, "FAIL: RIP-relative store with trailing immediate metadata\n");
        return 0;
    }
    if (!nr_hde64_decode_bounded(sib_absolute, sizeof(sib_absolute), &decoded) ||
        decoded.rip_relative || !decoded.has_sib ||
        decoded.displacement_offset != 4 || decoded.displacement_size != 4) {
        fprintf(stderr, "FAIL: SIB absolute displacement classification\n");
        return 0;
    }
    if (!nr_hde64_decode_bounded(addr_override, sizeof(addr_override), &decoded) ||
        decoded.rip_relative || !decoded.address_size_override) {
        fprintf(stderr, "FAIL: address-size override must not be classified RIP-relative\n");
        return 0;
    }
    if (!nr_hde64_decode_bounded(rbp_disp8, sizeof(rbp_disp8), &decoded) ||
        decoded.rip_relative || decoded.displacement_offset != 3 ||
        decoded.displacement_size != 1) {
        fprintf(stderr, "FAIL: disp8 metadata\n");
        return 0;
    }
    return 1;
}

int main(void) {
    const uint8_t prefix[] = {0x66};
    const uint8_t rex_opcode[] = {0x48};
    const uint8_t modrm[] = {0x48, 0x8b};
    const uint8_t displacement[] = {0x48, 0x8b, 0x05};
    const uint8_t immediate[] = {0xe8, 0x11, 0x22};
    int ok = test_metadata();
    ok &= raw_decoder_faults(prefix, sizeof(prefix), "operand prefix");
    ok &= raw_decoder_faults(rex_opcode, sizeof(rex_opcode), "REX prefix");
    ok &= raw_decoder_faults(modrm, sizeof(modrm), "ModRM");
    ok &= raw_decoder_faults(displacement, sizeof(displacement), "disp32");
    ok &= raw_decoder_faults(immediate, sizeof(immediate), "rel32 immediate");
    ok &= bounded_rejects(prefix, sizeof(prefix), "operand prefix");
    ok &= bounded_rejects(rex_opcode, sizeof(rex_opcode), "REX prefix");
    ok &= bounded_rejects(modrm, sizeof(modrm), "ModRM");
    ok &= bounded_rejects(displacement, sizeof(displacement), "disp32");
    ok &= bounded_rejects(immediate, sizeof(immediate), "rel32 immediate");
    if (!ok) return 1;
    puts("HDE64 raw characterization: five truncated-input guard-page faults reproduced.");
    puts("NR bounded decoder: five truncated windows rejected; relative/RIP metadata vectors pass.");
    puts("Input-span readability is a caller precondition; no live detour integration is enabled.");
    return 0;
}
