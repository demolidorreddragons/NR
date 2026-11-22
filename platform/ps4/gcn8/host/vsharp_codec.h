#ifndef PNE_GCN8_VSHARP_CODEC_H
#define PNE_GCN8_VSHARP_CODEC_H
#include <cstdint>
namespace pne::gcn8 {
enum class VSharpCodecError : std::uint8_t {
    none, null_output, address_range, address_alignment, stride_range,
    channel_range, numformat_range, dataformat_range, field_range
};
struct VSharpBufferFields {
    std::uint64_t gpu_address = 0;
    std::uint32_t stride = 0;
    std::uint32_t num_records = 0;
    std::uint8_t mtype_l1s = 0, mtype_l2 = 0, mtype = 0;
    std::uint8_t dstselx = 0, dstsely = 0, dstselz = 0, dstselw = 0;
    std::uint8_t numformat = 0, dataformat = 0;
    std::uint8_t elementsize = 0, indexstride = 0;
    bool cacheswizzle = false, swizzleen = false;
    bool addtiden = false, atc = false, hashen = false, heap = false;
    std::uint8_t type = 0;
};
// Mirrors the pinned OpenGNM sceGnmBufSetMemoryType bit encoding only;
// it does not assert that a given memory type is correct for a runtime workload.
void set_vsharp_memory_type(VSharpBufferFields& fields,
                            std::uint32_t memory_type,
                            bool l1cachebypass,
                            bool kcachebypass) noexcept;
// Packs the documented four GNM V# registers into four little-endian dwords.
// Enum meanings and runtime cache policy are deliberately outside this codec.
VSharpCodecError pack_vsharp_buffer(const VSharpBufferFields& fields,
                                    std::uint32_t words[4]) noexcept;
const char* vsharp_codec_error_string(VSharpCodecError error) noexcept;
}
#endif
