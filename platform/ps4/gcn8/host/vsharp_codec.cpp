#include "vsharp_codec.h"
namespace pne::gcn8 {
namespace {
constexpr bool fits(std::uint32_t value, unsigned bits) noexcept {
    return bits == 32 || value < (std::uint32_t{1} << bits);
}
}
VSharpCodecError pack_vsharp_buffer(const VSharpBufferFields& f,
                                    std::uint32_t words[4]) noexcept {
    if (!words) return VSharpCodecError::null_output;
    words[0] = words[1] = words[2] = words[3] = 0;
    if (f.gpu_address == 0 || f.gpu_address >= (std::uint64_t{1} << 44))
        return VSharpCodecError::address_range;
    if ((f.gpu_address & 3u) != 0) return VSharpCodecError::address_alignment;
    if (!fits(f.stride, 14)) return VSharpCodecError::stride_range;
    const std::uint32_t channels[] = {f.dstselx,f.dstsely,f.dstselz,f.dstselw};
    for (auto v : channels) if (!fits(v,3)) return VSharpCodecError::channel_range;
    if (!fits(f.numformat,3)) return VSharpCodecError::numformat_range;
    if (!fits(f.dataformat,4)) return VSharpCodecError::dataformat_range;
    if (!fits(f.mtype_l1s,2) || !fits(f.mtype_l2,2) || !fits(f.mtype,3) ||
        !fits(f.elementsize,2) || !fits(f.indexstride,2) || !fits(f.type,2))
        return VSharpCodecError::field_range;
    words[0] = static_cast<std::uint32_t>(f.gpu_address);
    words[1] = static_cast<std::uint32_t>((f.gpu_address >> 32) & 0xfffu)
        | (static_cast<std::uint32_t>(f.mtype_l1s) << 12)
        | (static_cast<std::uint32_t>(f.mtype_l2) << 14)
        | (f.stride << 16)
        | (static_cast<std::uint32_t>(f.cacheswizzle) << 30)
        | (static_cast<std::uint32_t>(f.swizzleen) << 31);
    words[2] = f.num_records;
    words[3] = static_cast<std::uint32_t>(f.dstselx)
        | (static_cast<std::uint32_t>(f.dstsely) << 3)
        | (static_cast<std::uint32_t>(f.dstselz) << 6)
        | (static_cast<std::uint32_t>(f.dstselw) << 9)
        | (static_cast<std::uint32_t>(f.numformat) << 12)
        | (static_cast<std::uint32_t>(f.dataformat) << 15)
        | (static_cast<std::uint32_t>(f.elementsize) << 19)
        | (static_cast<std::uint32_t>(f.indexstride) << 21)
        | (static_cast<std::uint32_t>(f.addtiden) << 23)
        | (static_cast<std::uint32_t>(f.atc) << 24)
        | (static_cast<std::uint32_t>(f.hashen) << 25)
        | (static_cast<std::uint32_t>(f.heap) << 26)
        | (static_cast<std::uint32_t>(f.mtype) << 27)
        | (static_cast<std::uint32_t>(f.type) << 30);
    return VSharpCodecError::none;
}
const char* vsharp_codec_error_string(VSharpCodecError e) noexcept {
    switch(e) {
    case VSharpCodecError::none: return "none";
    case VSharpCodecError::null_output: return "null output";
    case VSharpCodecError::address_range: return "GPU address must fit 44 bits and be non-zero";
    case VSharpCodecError::address_alignment: return "GPU address must be 4-byte aligned";
    case VSharpCodecError::stride_range: return "stride exceeds 14 bits";
    case VSharpCodecError::channel_range: return "channel selector exceeds 3 bits";
    case VSharpCodecError::numformat_range: return "numeric format exceeds 3 bits";
    case VSharpCodecError::dataformat_range: return "data format exceeds 4 bits";
    case VSharpCodecError::field_range: return "V# field exceeds documented bit width";
    }
    return "unknown";
}
}
