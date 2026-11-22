#pragma once

#include <stddef.h>
#include <stdint.h>
#include "HDE64.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum nr_hde64_relative_kind {
    NR_HDE64_RELATIVE_NONE = 0,
    NR_HDE64_RELATIVE_CALL,
    NR_HDE64_RELATIVE_JUMP,
    NR_HDE64_RELATIVE_CONDITIONAL,
    NR_HDE64_RELATIVE_LOOP,
    NR_HDE64_RELATIVE_UNSUPPORTED
} nr_hde64_relative_kind;

typedef struct nr_hde64_decoded {
    hde64s upstream;
    uint8_t length;
    uint8_t relative_offset;
    uint8_t relative_size;
    uint8_t displacement_offset;
    uint8_t displacement_size;
    uint8_t has_modrm;
    uint8_t has_sib;
    uint8_t address_size_override;
    uint8_t rip_relative;
    nr_hde64_relative_kind relative_kind;
} nr_hde64_decoded;

/*
 * Decode one x86-64 instruction from an explicitly bounded byte span.
 *
 * Contract: bytes[0..available) must already be readable and remain readable
 * for the duration of this call. This function cannot validate an arbitrary
 * pointer, pin a mapping, or prevent concurrent unmapping. It copies at most
 * 15 caller-declared bytes into a 32-byte zero-padded scratch window before
 * invoking the legacy, unbounded upstream decoder.
 *
 * Returns 1 only for a complete, non-error instruction wholly contained in
 * the supplied span; returns 0 for null/empty input, incomplete instructions,
 * decoder errors, or inconsistent metadata. A decoded relative instruction
 * may have kind NR_HDE64_RELATIVE_UNSUPPORTED; consumers must fail closed
 * unless they explicitly support that encoding.
 */
int nr_hde64_decode_bounded(const uint8_t *bytes, size_t available,
                            nr_hde64_decoded *out);

#ifdef __cplusplus
}
#endif
