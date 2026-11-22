#define _GNU_SOURCE
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "HDE64.h"

/* Test-only bounded adapter. The caller must supply a genuinely readable
 * byte count. Padding contains upstream speculative reads; overlong results
 * and all decoder errors are rejected.
 */
static int hde64_disasm_bounded(const void *code, size_t available, hde64s *out) {
    uint8_t window[32] = {0};
    hde64s decoded;
    if (!code || !out || available == 0) return 0;
    if (available > 15) available = 15;
    memcpy(window, code, available);
    unsigned int length = hde64_disasm(window, &decoded);
    if (length == 0 || length > available || (decoded.flags & F_ERROR)) return 0;
    *out = decoded;
    return 1;
}

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

static int bounded_adapter_rejects(const uint8_t *bytes, size_t length, const char *name) {
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
    hde64s decoded;
    int ok = !hde64_disasm_bounded(start, length, &decoded);
    munmap(region, (size_t)page_size * 2);
    if (!ok) fprintf(stderr, "FAIL: bounded adapter accepted truncated %s\n", name);
    return ok;
}

static int test_valid_decode(void) {
    hde64s hs;
    const uint8_t call_rel32[] = {0xe8, 0, 0, 0, 0};
    const uint8_t rip_load[] = {0x48, 0x8b, 0x05, 0, 0, 0, 0};
    if (!hde64_disasm_bounded(call_rel32, sizeof(call_rel32), &hs) ||
        hs.len != 5 || !(hs.flags & F_RELATIVE)) {
        fprintf(stderr, "FAIL: bounded rel32 CALL classification\n");
        return 0;
    }
    if (!hde64_disasm_bounded(rip_load, sizeof(rip_load), &hs) ||
        hs.len != 7 || !(hs.flags & F_DISP32) ||
        hs.modrm_mod != 0 || hs.modrm_rm != 5) {
        fprintf(stderr, "FAIL: bounded RIP-relative MOV structural classification\n");
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
    ok &= raw_decoder_faults(prefix, sizeof(prefix), "operand prefix");
    ok &= raw_decoder_faults(rex_opcode, sizeof(rex_opcode), "REX prefix");
    ok &= raw_decoder_faults(modrm, sizeof(modrm), "ModRM");
    ok &= raw_decoder_faults(displacement, sizeof(displacement), "disp32");
    ok &= raw_decoder_faults(immediate, sizeof(immediate), "rel32 immediate");
    ok &= bounded_adapter_rejects(prefix, sizeof(prefix), "operand prefix");
    ok &= bounded_adapter_rejects(rex_opcode, sizeof(rex_opcode), "REX prefix");
    ok &= bounded_adapter_rejects(modrm, sizeof(modrm), "ModRM");
    ok &= bounded_adapter_rejects(displacement, sizeof(displacement), "disp32");
    ok &= bounded_adapter_rejects(immediate, sizeof(immediate), "rel32 immediate");
    if (!ok) return 1;
    puts("HDE64 raw characterization: five truncated-input guard-page faults reproduced.");
    puts("Test-only bounded adapter: valid relative decodes pass; all five truncated windows rejected.");
    puts("Not production-integrated; caller must guarantee that the supplied byte count is readable.");
    return 0;
}
