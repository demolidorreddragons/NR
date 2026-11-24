#ifndef PNE_GCN8_KNOWN_ANSWER_CONTRACT_H
#define PNE_GCN8_KNOWN_ANSWER_CONTRACT_H

#include <cstddef>
#include <cstdint>

namespace pne::gcn8 {

// The Gate C smoke shader is dispatched as four 64-thread groups into a
// 256-element uint32 storage buffer. Each lane writes (global_id * 2) + 1.
inline constexpr std::size_t kKnownAnswerElementCount = 256;

std::uint32_t known_answer_value(std::uint32_t global_id) noexcept;
bool verify_known_answer(
    const std::uint32_t* actual,
    std::size_t element_count) noexcept;

} // namespace pne::gcn8

#endif
