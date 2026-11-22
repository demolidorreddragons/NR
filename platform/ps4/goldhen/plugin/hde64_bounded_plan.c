#include "hde64_bounded_plan.h"

#include <string.h>

int nr_hde64_plan_stolen_span(const uint8_t *bytes, size_t available,
                              size_t minimum_bytes,
                              nr_hde64_stolen_plan *out) {
    nr_hde64_stolen_plan plan;
    size_t offset = 0;

    if (!bytes || !out || minimum_bytes == 0 ||
        minimum_bytes > NR_HDE64_PLAN_MAX_BYTES ||
        available < minimum_bytes)
        return 0;

    if (available > NR_HDE64_PLAN_MAX_BYTES)
        available = NR_HDE64_PLAN_MAX_BYTES;

    memset(&plan, 0, sizeof(plan));
    while (offset < minimum_bytes) {
        nr_hde64_decoded decoded;
        size_t remaining = available - offset;

        if (plan.instruction_count >= NR_HDE64_PLAN_MAX_INSTRUCTIONS ||
            remaining == 0 ||
            !nr_hde64_decode_bounded(bytes + offset, remaining, &decoded))
            return 0;

        if (decoded.length == 0 ||
            decoded.relative_kind != NR_HDE64_RELATIVE_NONE ||
            decoded.rip_relative ||
            decoded.length > NR_HDE64_PLAN_MAX_BYTES - offset)
            return 0;

        nr_hde64_plan_entry *entry =
            &plan.instructions[plan.instruction_count];
        entry->offset = (uint8_t)offset;
        entry->length = decoded.length;
        entry->relative_kind = decoded.relative_kind;
        entry->rip_relative = decoded.rip_relative;

        offset += decoded.length;
        ++plan.instruction_count;
    }

    if (offset < minimum_bytes || offset > NR_HDE64_PLAN_MAX_BYTES)
        return 0;

    plan.stolen_length = (uint8_t)offset;
    *out = plan;
    return 1;
}
