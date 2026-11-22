#include "vsharp_codec.h"
#include <cassert>
#include <cstdint>
using namespace pne::gcn8;
static void expect_error(VSharpBufferFields fields, VSharpCodecError expected) {
    std::uint32_t words[4] = {1, 2, 3, 4};
    assert(pack_vsharp_buffer(fields, words) == expected);
    assert(words[0] == 0 && words[1] == 0 && words[2] == 0 && words[3] == 0);
}
int main() {
    VSharpBufferFields f{};
    f.gpu_address = 0x00000abc12345000ULL;
    f.stride = 4;
    f.num_records = 64;
    f.mtype_l1s = 1; f.mtype_l2 = 2; f.mtype = 3;
    f.dstselx = 0; f.dstsely = 1; f.dstselz = 2; f.dstselw = 3;
    f.numformat = 4; f.dataformat = 5; f.elementsize = 1; f.indexstride = 2;
    f.cacheswizzle = true; f.swizzleen = true; f.addtiden = true;
    f.atc = true; f.hashen = true; f.heap = true; f.type = 2;
    std::uint32_t w[4]{};
    assert(pack_vsharp_buffer(f, w) == VSharpCodecError::none);
    assert(w[0] == 0x12345000u);
    assert(w[1] == (0xabcu | (1u<<12) | (2u<<14) | (4u<<16) | (1u<<30) | (1u<<31)));
    assert(w[2] == 64u);
    assert(w[3] == (0u | (1u<<3) | (2u<<6) | (3u<<9) | (4u<<12) |
        (5u<<15) | (1u<<19) | (2u<<21) | (1u<<23) | (1u<<24) |
        (1u<<25) | (1u<<26) | (3u<<27) | (2u<<30)));
    assert(pack_vsharp_buffer(f, nullptr) == VSharpCodecError::null_output);

    // Exact R32_FLOAT V# fields used by the PS4 compute smoke, from the
    // pinned OpenGNM GNM_FMT_R32_FLOAT definition (surface=4, num=7,
    // selectors X/0/0/1). Memory-type fields mirror sceGnmBufSetMemoryType.
    f = {};
    f.gpu_address = 0x0000000012345000ULL;
    f.stride = 4;
    f.num_records = 64;
    set_vsharp_memory_type(f, 0x10u, false, false); // GNM_MEMORY_READONLY.
    assert(f.mtype == 4 && f.mtype_l1s == 0 && f.mtype_l2 == 0);
    f.dstselx = 4; f.dstsely = 0; f.dstselz = 0; f.dstselw = 1;
    f.numformat = 7; f.dataformat = 4;
    assert(pack_vsharp_buffer(f, w) == VSharpCodecError::none);
    assert(w[0] == 0x12345000u && w[1] == 0x00040000u &&
           w[2] == 64u && w[3] == 0x20027204u);

    f.gpu_address = 0x0000000012346000ULL;
    set_vsharp_memory_type(f, 0x6fu, false, false); // GNM_MEMORY_UNCACHED.
    assert(f.mtype == 0 && f.mtype_l1s == 3 && f.mtype_l2 == 3);
    assert(pack_vsharp_buffer(f, w) == VSharpCodecError::none);
    assert(w[0] == 0x12346000u && w[1] == 0x0004f000u &&
           w[2] == 64u && w[3] == 0x00027204u);

    // Maximum legal address and field values must pack without truncation.
    f = {};
    f.gpu_address = (std::uint64_t{1} << 44) - 4;
    f.stride = (1u << 14) - 1;
    f.num_records = 0xffffffffu;
    f.mtype_l1s = 3; f.mtype_l2 = 3; f.mtype = 7;
    f.dstselx = f.dstsely = f.dstselz = f.dstselw = 7;
    f.numformat = 7; f.dataformat = 15; f.elementsize = 3;
    f.indexstride = 3; f.addtiden = true; f.atc = true;
    f.hashen = true; f.heap = true; f.type = 3;
    assert(pack_vsharp_buffer(f, w) == VSharpCodecError::none);
    assert(w[0] == 0xfffffffcu);
    assert((w[1] & 0xfffu) == 0xfffu);
    assert(((w[1] >> 16) & 0x3fffu) == 0x3fffu);
    assert(w[2] == 0xffffffffu);
    assert(w[3] == 0xffffffffu);

    f.gpu_address = 0;
    expect_error(f, VSharpCodecError::address_range);
    f.gpu_address = std::uint64_t{1} << 44;
    expect_error(f, VSharpCodecError::address_range);
    f.gpu_address = 0x1001;
    expect_error(f, VSharpCodecError::address_alignment);
    f.gpu_address = 0x1000; f.stride = 1u << 14;
    expect_error(f, VSharpCodecError::stride_range);
    f.stride = 4; f.dstselw = 8;
    expect_error(f, VSharpCodecError::channel_range);
    f.dstselw = 3; f.numformat = 8;
    expect_error(f, VSharpCodecError::numformat_range);
    f.numformat = 4; f.dataformat = 16;
    expect_error(f, VSharpCodecError::dataformat_range);
    f.dataformat = 5; f.mtype_l1s = 4;
    expect_error(f, VSharpCodecError::field_range);
    f.mtype_l1s = 0; f.mtype_l2 = 4;
    expect_error(f, VSharpCodecError::field_range);
    f.mtype_l2 = 0; f.mtype = 8;
    expect_error(f, VSharpCodecError::field_range);
    f.mtype = 0; f.elementsize = 4;
    expect_error(f, VSharpCodecError::field_range);
    f.elementsize = 0; f.indexstride = 4;
    expect_error(f, VSharpCodecError::field_range);
    f.indexstride = 0; f.type = 4;
    expect_error(f, VSharpCodecError::field_range);
    return 0;
}
