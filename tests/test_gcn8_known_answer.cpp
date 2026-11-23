#include "known_answer_contract.h"

#include <array>
#include <cassert>
#include <cstdint>

using namespace pne::gcn8;

int main() {
    std::array<std::uint32_t, kKnownAnswerElementCount> output{};
    for (std::uint32_t i = 0; i < output.size(); ++i)
        output[i] = known_answer_value(i);

    assert(output.front() == 1u);
    assert(output.back() == 511u);
    assert(verify_known_answer(output.data(), output.size()));
    assert(!verify_known_answer(nullptr, output.size()));
    assert(!verify_known_answer(output.data(), output.size() - 1));
    assert(!verify_known_answer(output.data(), output.size() + 1));

    output[127] ^= 1u;
    assert(!verify_known_answer(output.data(), output.size()));
    output[127] ^= 1u;
    assert(verify_known_answer(output.data(), output.size()));
    return 0;
}
