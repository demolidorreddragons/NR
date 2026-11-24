#include "upload_confirmation.h"

#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "CHECK failed: %s at %s:%d\\n", #x, __FILE__, __LINE__); return 1; } } while (0)

int main() {
    using namespace pne::upload;
    const std::uint8_t abc[] = {'a','b','c'};
    CHECK(sha256(abc, sizeof(abc)) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256(nullptr, 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const char long_vector[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK(sha256(reinterpret_cast<const std::uint8_t*>(long_vector), std::strlen(long_vector)) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    // Exercise SHA-256 padding boundaries and the first multi-block boundary.
    const std::array<const char*, 9> boundary_hashes{{
        "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318",
        "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a",
        "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34",
        "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb",
        "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0",
        "31eba51c313a5c08226adf18d4a359cfdfd8d2e816b13f4af952f7ea6584dcfb",
        "2f3d335432c70b580af0e8e1b3674a7c020d683aa5f73aaaedfdc55af904c21c",
        "c57e9278af78fa3cab38667bef4ce29d783787a2f731d4e12200270f0c32320a",
        "6836cf13bac400e9105071cd6af47084dfacad4e5e302c94bfed24e013afb73e"
    }};
    const std::array<std::size_t, 9> boundary_sizes{{55, 56, 63, 64, 65, 119, 120, 127, 128}};
    std::vector<std::uint8_t> repeated(128, static_cast<std::uint8_t>('a'));
    for (std::size_t i = 0; i < boundary_sizes.size(); ++i) {
        CHECK(sha256(repeated.data(), boundary_sizes[i]) == boundary_hashes[i]);
    }

    std::vector<FileIdentity> preview_files{
        {"z.bmp", 20, "digest-z"},
        {"frametimes.csv", 12, "digest-csv"}
    };
    std::string preview;
    const std::string expected_preview =
        "PNE upload preview: 2 files, 32 bytes total\\n"
        "frametimes.csv: 12 bytes\\n"
        "z.bmp: 20 bytes\\n";
    CHECK(build_preview(preview_files, expected_preview.size() + 1, preview));
    CHECK(preview == expected_preview);
    CHECK(!build_preview(preview_files, expected_preview.size(), preview));
    CHECK(preview.empty());
    auto duplicate_preview = preview_files;
    duplicate_preview.push_back({"z.bmp", 20, "duplicate"});
    CHECK(!build_preview(duplicate_preview, 1024, preview));
    CHECK(preview.empty());
    auto unsafe_preview = preview_files;
    unsafe_preview[0].relative_name = "bad\\nname.bmp";
    CHECK(!build_preview(unsafe_preview, 1024, preview));
    CHECK(preview.empty());

    Confirmation confirmation;
    std::vector<FileIdentity> first{{"one.bmp", 12, "digest-a"}, {"two.bmp", 20, "digest-b"}};
    CHECK(confirmation.observe(first) == ConfirmationResult::armed);
    CHECK(confirmation.pending());
    auto reordered = first; std::swap(reordered[0], reordered[1]);
    CHECK(confirmation.matches(reordered));
    CHECK(confirmation.observe(reordered) == ConfirmationResult::confirmed);
    CHECK(!confirmation.pending());

    // Same aggregate size/count but a different filename cannot inherit confirmation.
    CHECK(confirmation.observe(first) == ConfirmationResult::armed);
    auto same_size_different_names = first;
    same_size_different_names[0].relative_name = "other.bmp";
    CHECK(!confirmation.matches(same_size_different_names));
    CHECK(confirmation.observe(same_size_different_names) == ConfirmationResult::armed);
    CHECK(confirmation.observe(same_size_different_names) == ConfirmationResult::confirmed);

    // Same name and size with different bytes, or an added/removed file, must re-arm.
    CHECK(confirmation.observe(first) == ConfirmationResult::armed);
    auto changed_content = first; changed_content[0].sha256 = "different-content";
    CHECK(confirmation.observe(changed_content) == ConfirmationResult::armed);
    CHECK(confirmation.observe(changed_content) == ConfirmationResult::confirmed);

    CHECK(confirmation.observe(first) == ConfirmationResult::armed);
    auto added_file = first;
    added_file.push_back({"three.bmp", 7, "digest-c"});
    CHECK(!confirmation.matches(added_file));
    CHECK(confirmation.observe(added_file) == ConfirmationResult::armed);
    CHECK(confirmation.observe(added_file) == ConfirmationResult::confirmed);

    CHECK(confirmation.observe(added_file) == ConfirmationResult::armed);
    auto removed_file = added_file;
    removed_file.pop_back();
    CHECK(!confirmation.matches(removed_file));
    CHECK(confirmation.observe(removed_file) == ConfirmationResult::armed);

    confirmation.invalidate();
    CHECK(!confirmation.pending());
    CHECK(confirmation.observe(removed_file) == ConfirmationResult::armed);
    return 0;
}
