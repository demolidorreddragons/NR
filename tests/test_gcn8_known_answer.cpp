#include "known_answer_contract.h"

#include <array>
#include <cstdio>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "CHECK failed: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)
#include <cstdint>

using namespace pne::gcn8;

int main() {
    // Construct expected GPU output independently of the production oracle.
    std::array<std::uint32_t, kKnownAnswerElementCount> output{};
    for (std::uint32_t i = 0; i < output.size(); ++i)
        output[i] = (i * 2u) + 1u;

    CHECK(output.front() == 1u);
    CHECK(output.back() == 511u);
    CHECK(known_answer_value(0) == 1u);
    CHECK(known_answer_value(127) == 255u);
    CHECK(known_answer_value(255) == 511u);
    CHECK(verify_known_answer(output.data(), output.size()));
    CHECK(!verify_known_answer(nullptr, output.size()));
    CHECK(!verify_known_answer(output.data(), output.size() - 1));
    CHECK(!verify_known_answer(output.data(), output.size() + 1));

    // A single corrupted GPU result must fail the complete comparison.
    output[127] ^= 1u;
    CHECK(!verify_known_answer(output.data(), output.size()));
    output[127] ^= 1u;
    CHECK(verify_known_answer(output.data(), output.size()));
    return 0;
}
