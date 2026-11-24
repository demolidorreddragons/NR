#include "upload_confirmation.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace pne::upload {
namespace {
constexpr std::uint32_t kRound[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
constexpr std::uint32_t rotate(std::uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
void transform(std::array<std::uint32_t, 8>& h, const std::uint8_t* block) {
    std::uint32_t w[64];
    for (unsigned i=0;i<16;i++) w[i]=(std::uint32_t(block[i*4])<<24)|(std::uint32_t(block[i*4+1])<<16)|(std::uint32_t(block[i*4+2])<<8)|block[i*4+3];
    for (unsigned i=16;i<64;i++) { auto a=rotate(w[i-15],7)^rotate(w[i-15],18)^(w[i-15]>>3); auto b=rotate(w[i-2],17)^rotate(w[i-2],19)^(w[i-2]>>10); w[i]=w[i-16]+a+w[i-7]+b; }
    auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],x=h[7];
    for (unsigned i=0;i<64;i++) { auto s1=rotate(e,6)^rotate(e,11)^rotate(e,25); auto ch=(e&f)^(~e&g); auto t1=x+s1+ch+kRound[i]+w[i]; auto s0=rotate(a,2)^rotate(a,13)^rotate(a,22); auto maj=(a&b)^(a&c)^(b&c); auto t2=s0+maj; x=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2; }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=x;
}

bool safe_name(const std::string& name) {
    if (name.empty()) return false;
    for (unsigned char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
            return false;
    }
    return true;
}

bool append_bounded(std::string& output, const std::string& part, std::size_t max_length) {
    if (max_length == 0 || output.size() >= max_length ||
        part.size() >= max_length - output.size())
        return false;
    output += part;
    return true;
}
}

std::string sha256(const std::uint8_t* data, std::size_t size) {
    if (!data && size) return {};
    std::array<std::uint32_t,8> h{{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
    std::size_t offset=0;
    while (size-offset>=64) { transform(h,data+offset); offset+=64; }
    std::uint8_t tail[128]{}; const auto remain=size-offset;
    for (std::size_t i=0;i<remain;i++) tail[i]=data[offset+i];
    tail[remain]=0x80; const std::size_t padded=remain<56?64:128;
    const std::uint64_t bits=static_cast<std::uint64_t>(size)*8;
    for (unsigned i=0;i<8;i++) tail[padded-1-i]=static_cast<std::uint8_t>(bits>>(i*8));
    transform(h,tail); if(padded==128) transform(h,tail+64);
    static constexpr char hex[]="0123456789abcdef"; std::string out;out.reserve(64);
    for(auto word:h) for(int shift=28;shift>=0;shift-=4) out+=hex[(word>>shift)&15];
    return out;
}

ConfirmationResult Confirmation::observe(std::vector<FileIdentity> files) {
    std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.relative_name<b.relative_name;});
    if (pending_ && files_==files) { invalidate(); return ConfirmationResult::confirmed; }
    files_=std::move(files); pending_=true; return ConfirmationResult::armed;
}
bool Confirmation::matches(const std::vector<FileIdentity>& files) const {
    auto sorted=files;
    std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return a.relative_name<b.relative_name;});
    return pending_ && files_==sorted;
}
void Confirmation::invalidate() noexcept { pending_=false; files_.clear(); }
bool same_files(std::vector<FileIdentity> left, std::vector<FileIdentity> right) {
    const auto by_name=[](const auto& a,const auto& b){return a.relative_name<b.relative_name;};
    std::sort(left.begin(),left.end(),by_name); std::sort(right.begin(),right.end(),by_name);
    return left==right;
}

bool build_preview(const std::vector<FileIdentity>& files,
                   std::size_t max_length,
                   std::string& output) {
    output.clear();
    if (files.empty() || max_length == 0) return false;

    auto sorted = files;
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.relative_name < b.relative_name; });

    std::uint64_t total_bytes = 0;
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        const auto& file = sorted[i];
        if (!safe_name(file.relative_name) || file.size == 0 ||
            (i != 0 && sorted[i - 1].relative_name == file.relative_name) ||
            file.size > std::numeric_limits<std::uint64_t>::max() - total_bytes) {
            output.clear();
            return false;
        }
        total_bytes += file.size;
    }

    if (!append_bounded(output,
            "PNE upload preview: " + std::to_string(sorted.size()) +
            " files, " + std::to_string(total_bytes) + " bytes total\n",
            max_length)) {
        output.clear();
        return false;
    }

    for (const auto& file : sorted) {
        if (!append_bounded(output,
                file.relative_name + ": " + std::to_string(file.size) + " bytes\n",
                max_length)) {
            output.clear();
            return false;
        }
    }
    return true;
}
}
