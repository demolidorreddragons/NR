#include "known_answer_contract.h"

#include <array>
#include <cassert>
#include <cstdint>

using namespace pne::gcn8;

int main() {
    // Construct expected GPU output independently of the production oracle.
    std::array<std::uint32_t, kKnownAnswerElementCount> output{};
    for (std::uint32_t i = 0; i < output.size(); ++i)
        output[i] = (i * 2u) + 1u;

    assert(output.front() == 1u);
    assert(output.back() == 511u);
    assert(known_answer_value(0) == 1u);
    assert(known_answer_value(127) == 255u);
    assert(known_answer_value(255) == 511u);
    assert(verify_known_answer(output.data(), output.size()));
    assert(!verify_known_answer(nullptr, output.size()));
    assert(!verify_known_answer(output.data(), output.size() - 1));
    assert(!verify_known_answer(output.data(), output.size() + 1));

    // A single corrupted GPU result must fail the complete comparison.
    output[127] ^= 1u;
    assert(!verify_known_answer(output.data(), output.size()));
    output[127] ^= 1u;
    assert(verify_known_answer(output.data(), output.size()));
    return 0;
}
