#ifndef PNE_GCN8_COMPUTE_CONTRACT_H
#define PNE_GCN8_COMPUTE_CONTRACT_H

#include <cstddef>
#include <cstdint>

namespace pne::gcn8 {

struct GemmParams {
    std::uint32_t m;
    std::uint32_t n;
    std::uint32_t k;
    float alpha;
    float beta;
};

static_assert(sizeof(GemmParams) == 20, "GLSL push-constant ABI must be 20 bytes");
static_assert(offsetof(GemmParams, alpha) == 12, "GLSL alpha offset mismatch");
static_assert(offsetof(GemmParams, beta) == 16, "GLSL beta offset mismatch");

struct BufferView {
    const void* data;
    std::size_t size_bytes;
};

struct GemmDispatch {
    GemmParams params;
    std::uint32_t groups_x;
    std::uint32_t groups_y;
    std::size_t a_required_bytes;
    std::size_t b_required_bytes;
    std::size_t c_required_bytes;
    std::size_t out_required_bytes;
};

enum class ContractError {
    none,
    null_dispatch,
    zero_output_dimension,
    shader_index_overflow,
    size_overflow,
    null_buffer,
    buffer_too_small,
    output_aliases_input,
    invalid_scalar,
    invalid_workgroup_size
};

// General contract for a kernel whose workgroup covers tile_width x tile_height outputs.
// The 16x16 wrapper below is specifically for gemm_fp32_wave64_tiled.comp.
ContractError make_gemm_dispatch_for_tile(
    const GemmParams& params,
    const BufferView& a,
    const BufferView& b,
    const BufferView& c,
    const BufferView& out,
    std::uint32_t tile_width,
    std::uint32_t tile_height,
    GemmDispatch* dispatch);

// 16x16 tiled GCN8 GEMM dispatch.
ContractError make_gemm_dispatch(
    const GemmParams& params,
    const BufferView& a,
    const BufferView& b,
    const BufferView& c,
    const BufferView& out,
    GemmDispatch* dispatch);

const char* contract_error_string(ContractError error) noexcept;

} // namespace pne::gcn8

#endif
