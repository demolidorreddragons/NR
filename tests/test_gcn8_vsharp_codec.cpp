#include "vsharp_codec.h"
#include <cassert>
#include <cstdint>
using namespace pne::gcn8;
int main() {
    VSharpBufferFields f{};
    f.gpu_address=0x00000abc12345000ULL; f.stride=4; f.num_records=64;
    f.mtype_l1s=1; f.mtype_l2=2; f.mtype=3;
    f.dstselx=0; f.dstsely=1; f.dstselz=2; f.dstselw=3;
    f.numformat=4; f.dataformat=5; f.elementsize=1; f.indexstride=2;
    f.cacheswizzle=true; f.swizzleen=true; f.addtiden=true; f.atc=true;
    f.hashen=true; f.heap=true; f.type=2;
    std::uint32_t w[4]{};
    assert(pack_vsharp_buffer(f,w)==VSharpCodecError::none);
    assert(w[0]==0x12345000u);
    assert(w[1]==(0xabcu | (1u<<12) | (2u<<14) | (4u<<16) | (1u<<30) | (1u<<31)));
    assert(w[2]==64u);
    assert(w[3]==(0u | (1u<<3) | (2u<<6) | (3u<<9) | (4u<<12) |
        (5u<<15) | (1u<<19) | (2u<<21) | (1u<<23) | (1u<<24) |
        (1u<<25) | (1u<<26) | (3u<<27) | (2u<<30)));
    assert(pack_vsharp_buffer(f,nullptr)==VSharpCodecError::null_output);
    f.gpu_address=0; assert(pack_vsharp_buffer(f,w)==VSharpCodecError::address_range);
    f.gpu_address=0x1001; assert(pack_vsharp_buffer(f,w)==VSharpCodecError::address_alignment);
    f.gpu_address=0x1000; f.stride=1u<<14; assert(pack_vsharp_buffer(f,w)==VSharpCodecError::stride_range);
    f.stride=4; f.dstselw=8; assert(pack_vsharp_buffer(f,w)==VSharpCodecError::channel_range);
    f.dstselw=3; f.numformat=8; assert(pack_vsharp_buffer(f,w)==VSharpCodecError::numformat_range);
    f.numformat=4; f.dataformat=16; assert(pack_vsharp_buffer(f,w)==VSharpCodecError::dataformat_range);
    f.dataformat=5; f.mtype=8; assert(pack_vsharp_buffer(f,w)==VSharpCodecError::field_range);
    return 0;
}
