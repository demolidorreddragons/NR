#define _GNU_SOURCE
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "HDE64.h"

static int expect_guard_fault(const uint8_t *bytes, size_t length, const char *name) {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0 || length == 0 || length > (size_t)page_size) return 0;
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
    if (!ok) fprintf(stderr, "FAIL: truncated %s did not fault at guard page (status=%d)\n", name, status);
    return ok;
}

static int test_valid_decode(void) {
    hde64s hs;
    const uint8_t call_rel32[] = {0xe8, 0, 0, 0, 0};
    const uint8_t rip_load[] = {0x48, 0x8b, 0x05, 0, 0, 0, 0};
    if (hde64_disasm(call_rel32, &hs) != 5 || !(hs.flags & F_RELATIVE)) {
        fprintf(stderr, "FAIL: rel32 CALL classification\n");
        return 0;
    }
    if (hde64_disasm(rip_load, &hs) != 7 || !(hs.flags & F_DISP32) ||
        hs.modrm_mod != 0 || hs.modrm_rm != 5) {
        fprintf(stderr, "FAIL: RIP-relative MOV structural classification\n");
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
    int ok = test_valid_decode();
    ok &= expect_guard_fault(prefix, sizeof(prefix), "operand prefix");
    ok &= expect_guard_fault(rex_opcode, sizeof(rex_opcode), "REX prefix");
    ok &= expect_guard_fault(modrm, sizeof(modrm), "ModRM");
    ok &= expect_guard_fault(displacement, sizeof(displacement), "disp32");
    ok &= expect_guard_fault(immediate, sizeof(immediate), "rel32 immediate");
    if (!ok) return 1;
    puts("HDE64 characterization: valid relative decode PASS; five truncated-input guard-page fault cases reproduced.");
    puts("This is a negative characterization of the unbounded upstream decoder, NOT a safety pass.");
    return 0;
}
