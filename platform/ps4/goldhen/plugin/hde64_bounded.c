#include "hde64_bounded.h"

#include <string.h>

#define NR_HDE64_MAX_INSN 15u

static unsigned int flag_size(uint32_t flags, uint32_t f8,
                              uint32_t f16, uint32_t f32, uint32_t f64) {
    unsigned int count = !!(flags & f8) + !!(flags & f16) +
                         !!(flags & f32) + !!(flags & f64);
    if (count != 1) return 0;
    if (flags & f8) return 1;
    if (flags & f16) return 2;
    if (flags & f32) return 4;
    return 8;
}

static nr_hde64_relative_kind classify_relative(const hde64s *hs,
                                                 unsigned int relative_size) {
    if (!hs || !(hs->flags & F_RELATIVE)) return NR_HDE64_RELATIVE_NONE;
    if (relative_size == 1) {
        if (hs->opcode == 0xeb) return NR_HDE64_RELATIVE_JUMP;
        if (hs->opcode >= 0x70 && hs->opcode <= 0x7f)
            return NR_HDE64_RELATIVE_CONDITIONAL;
        if (hs->opcode >= 0xe0 && hs->opcode <= 0xe3)
            return NR_HDE64_RELATIVE_LOOP;
    }
    if (relative_size == 4) {
        if (hs->opcode == 0xe8) return NR_HDE64_RELATIVE_CALL;
        if (hs->opcode == 0xe9) return NR_HDE64_RELATIVE_JUMP;
        if (hs->opcode == 0x0f && hs->opcode2 >= 0x80 &&
            hs->opcode2 <= 0x8f)
            return NR_HDE64_RELATIVE_CONDITIONAL;
    }
    return NR_HDE64_RELATIVE_UNSUPPORTED;
}

int nr_hde64_decode_bounded(const uint8_t *bytes, size_t available,
                            nr_hde64_decoded *out) {
    uint8_t window[32] = {0};
    hde64s decoded;
    nr_hde64_decoded result;
    unsigned int length;
    unsigned int immediate_size;
    unsigned int displacement_size;

    if (!bytes || !out || available == 0) return 0;
    if (available > NR_HDE64_MAX_INSN) available = NR_HDE64_MAX_INSN;

    memcpy(window, bytes, available);
    length = hde64_disasm(window, &decoded);
    if (length == 0 || length > NR_HDE64_MAX_INSN ||
        length > available || decoded.len != length ||
        (decoded.flags & F_ERROR))
        return 0;

    memset(&result, 0, sizeof(result));
    result.upstream = decoded;
    result.length = (uint8_t)length;
    result.has_modrm = (decoded.flags & F_MODRM) != 0;
    result.has_sib = (decoded.flags & F_SIB) != 0;
    result.address_size_override = (decoded.flags & F_PREFIX_67) != 0;

    if (decoded.flags & F_RELATIVE) {
        immediate_size = flag_size(decoded.flags, F_IMM8, F_IMM16,
                                    F_IMM32, F_IMM64);
        if (immediate_size == 0 || immediate_size > length) return 0;
        result.relative_size = (uint8_t)immediate_size;
        result.relative_offset = (uint8_t)(length - immediate_size);
        result.relative_kind = classify_relative(&decoded, immediate_size);
    }

    displacement_size = flag_size(decoded.flags, F_DISP8, F_DISP16,
                                   F_DISP32, 0);
    if (decoded.flags & (F_DISP8 | F_DISP16 | F_DISP32)) {
        if (displacement_size == 0 || displacement_size > length)
            return 0;
        immediate_size = 0;
        if (decoded.flags & (F_IMM8 | F_IMM16 | F_IMM32 | F_IMM64)) {
            immediate_size = flag_size(decoded.flags, F_IMM8, F_IMM16,
                                        F_IMM32, F_IMM64);
            if (immediate_size == 0) return 0;
        }
        if (displacement_size + immediate_size > length) return 0;
        result.displacement_size = (uint8_t)displacement_size;
        result.displacement_offset =
            (uint8_t)(length - displacement_size - immediate_size);
    }

    result.rip_relative =
        (uint8_t)(result.has_modrm && !result.address_size_override &&
                  decoded.modrm_mod == 0 && decoded.modrm_rm == 5 &&
                  displacement_size == 4 &&
                  (decoded.flags & F_DISP32) != 0 && !result.has_sib);

    *out = result;
    return 1;
}
