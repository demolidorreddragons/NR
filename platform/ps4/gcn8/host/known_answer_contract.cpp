#include "known_answer_contract.h"

namespace pne::gcn8 {

std::uint32_t known_answer_value(std::uint32_t global_id) noexcept {
    return global_id * 2u + 1u;
}

bool verify_known_answer(
    const std::uint32_t* actual,
    std::size_t element_count) noexcept {
    if (!actual || element_count != kKnownAnswerElementCount)
        return false;

    for (std::size_t i = 0; i < kKnownAnswerElementCount; ++i) {
        if (actual[i] != known_answer_value(static_cast<std::uint32_t>(i)))
            return false;
    }
    return true;
}

} // namespace pne::gcn8
