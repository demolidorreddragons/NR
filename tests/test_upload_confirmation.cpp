#include "upload_confirmation.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "CHECK failed: %s at %s:%d\n", #x, __FILE__, __LINE__); return 1; } } while (0)

int main() {
    using namespace pne::upload;
    const std::uint8_t abc[] = {'a','b','c'};
    CHECK(sha256(abc, sizeof(abc)) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256(nullptr, 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const char long_vector[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK(sha256(reinterpret_cast<const std::uint8_t*>(long_vector), std::strlen(long_vector)) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    Confirmation confirmation;
    std::vector<FileIdentity> first{{"one.bmp", 12, "digest-a"}, {"two.bmp", 20, "digest-b"}};
    CHECK(confirmation.observe(first) == ConfirmationResult::armed);
    CHECK(confirmation.pending());
    auto reordered = first; std::swap(reordered[0], reordered[1]);
    CHECK(confirmation.matches(reordered));
    CHECK(confirmation.observe(reordered) == ConfirmationResult::confirmed);
    CHECK(!confirmation.pending());

    CHECK(confirmation.observe(first) == ConfirmationResult::armed);
    auto same_size_different_names = first;
    same_size_different_names[0].relative_name = "other.bmp";
    CHECK(!confirmation.matches(same_size_different_names));
    CHECK(confirmation.observe(same_size_different_names) == ConfirmationResult::armed);
    CHECK(confirmation.observe(same_size_different_names) == ConfirmationResult::confirmed);

    CHECK(confirmation.observe(first) == ConfirmationResult::armed);
    auto changed_content = first; changed_content[0].sha256 = "different-content";
    CHECK(confirmation.observe(changed_content) == ConfirmationResult::armed);
    CHECK(confirmation.observe(changed_content) == ConfirmationResult::confirmed);
    confirmation.invalidate();
    CHECK(!confirmation.pending());
    CHECK(confirmation.observe(changed_content) == ConfirmationResult::armed);
    return 0;
}
