#pragma once

#include <stddef.h>
#include <stdint.h>
#include "hde64_bounded.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NR_HDE64_PLAN_MAX_BYTES 64u
#define NR_HDE64_PLAN_MAX_INSTRUCTIONS 16u

typedef struct nr_hde64_plan_entry {
    uint8_t offset;
    uint8_t length;
    nr_hde64_relative_kind relative_kind;
    uint8_t rip_relative;
} nr_hde64_plan_entry;

typedef struct nr_hde64_stolen_plan {
    uint8_t stolen_length;
    uint8_t instruction_count;
    nr_hde64_plan_entry instructions[NR_HDE64_PLAN_MAX_INSTRUCTIONS];
} nr_hde64_stolen_plan;

/*
 * Build a read-only preflight plan for a prologue span. The planner accepts
 * only ordinary instructions: every relative control-flow instruction,
 * unsupported relative encoding, and RIP-relative memory operand is rejected.
 * No target memory is changed and no trampoline bytes are generated.
 *
 * Contract: bytes[0..available) must be readable and stable for this call.
 * The caller is responsible for establishing that span; this API does not
 * validate pointers, mappings, permissions, or concurrent mapping lifetime.
 *
 * minimum_bytes is normally the target patch width (14 for the current x64
 * absolute jump). The planner may return a larger instruction-aligned span.
 * Returns 1 on a complete safe-to-copy preflight plan, otherwise 0.
 */
int nr_hde64_plan_stolen_span(const uint8_t *bytes, size_t available,
                              size_t minimum_bytes,
                              nr_hde64_stolen_plan *out);

#ifdef __cplusplus
}
#endif
