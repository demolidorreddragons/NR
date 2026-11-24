#ifndef PNE_UPLOAD_CONFIRMATION_H
#define PNE_UPLOAD_CONFIRMATION_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pne::upload {

struct FileIdentity {
    std::string relative_name;
    std::uint64_t size = 0;
    std::string sha256;
    bool operator==(const FileIdentity& other) const noexcept {
        return relative_name == other.relative_name && size == other.size && sha256 == other.sha256;
    }
};

enum class ConfirmationResult { armed, confirmed };

// Confirmation is bound to sorted relative names, byte counts, and SHA-256.
class Confirmation final {
public:
    ConfirmationResult observe(std::vector<FileIdentity> files);
    bool matches(const std::vector<FileIdentity>& files) const;
    void invalidate() noexcept;
    bool pending() const noexcept { return pending_; }

private:
    bool pending_ = false;
    std::vector<FileIdentity> files_;
};

std::string sha256(const std::uint8_t* data, std::size_t size);
bool same_files(std::vector<FileIdentity> left, std::vector<FileIdentity> right);

// Produces a complete, sorted preview. max_length includes the trailing NUL.
bool build_preview(const std::vector<FileIdentity>& files,
                   std::size_t max_length,
                   std::string& output);

} // namespace pne::upload

#endif
