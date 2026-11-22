#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include "hde64_bounded.h"

static uint32_t next_random(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

int main(void) {
    long page_size = sysconf(_SC_PAGESIZE);
    uint8_t *region;
    uint32_t rng = 0x4e525836u;
    unsigned long accepted = 0;
    unsigned long rejected = 0;

    if (page_size <= 0) return 1;
    region = mmap(NULL, (size_t)page_size * 2, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) return 1;
    if (mprotect(region + page_size, (size_t)page_size, PROT_NONE) != 0) {
        munmap(region, (size_t)page_size * 2);
        return 1;
    }

    for (size_t length = 1; length <= 15; ++length) {
        for (unsigned int iteration = 0; iteration < 10000; ++iteration) {
            uint8_t *span = region + page_size - length;
            nr_hde64_decoded decoded;
            for (size_t i = 0; i < length; ++i)
                span[i] = (uint8_t)next_random(&rng);

            if (nr_hde64_decode_bounded(span, length, &decoded)) {
                if (decoded.length == 0 || decoded.length > length ||
                    decoded.upstream.len != decoded.length ||
                    (decoded.relative_size &&
                     (size_t)decoded.relative_offset + decoded.relative_size > decoded.length) ||
                    (decoded.displacement_size &&
                     (size_t)decoded.displacement_offset + decoded.displacement_size > decoded.length)) {
                    fprintf(stderr, "FAIL: inconsistent metadata length=%zu iteration=%u\n",
                            length, iteration);
                    munmap(region, (size_t)page_size * 2);
                    return 1;
                }
                ++accepted;
            } else {
                ++rejected;
            }
        }
    }

    munmap(region, (size_t)page_size * 2);
    printf("Bounded HDE64 randomized guard-page sweep: 150000 spans; accepted=%lu rejected=%lu; no out-of-span read.\n",
           accepted, rejected);
    return 0;
}
