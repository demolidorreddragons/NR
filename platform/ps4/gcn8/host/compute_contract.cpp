#include "compute_contract.h"

#include <cmath>
#include <cstdint>
#include <limits>

namespace pne::gcn8 {
namespace {

bool checked_mul(std::size_t lhs, std::size_t rhs, std::size_t* result) {
    if (rhs != 0 && lhs > std::numeric_limits<std::size_t>::max() / rhs) {
        return false;
    }
    *result = lhs * rhs;
    return true;
}

bool ranges_overlap(const void* lhs, std::size_t lhs_size,
                    const void* rhs, std::size_t rhs_size) {
    const auto a = reinterpret_cast<std::uintptr_t>(lhs);
    const auto b = reinterpret_cast<std::uintptr_t>(rhs);
    if (lhs_size == 0 || rhs_size == 0) return false;
    // Treat address-end overflow conservatively as overlap.
    if (a > std::numeric_limits<std::uintptr_t>::max() - lhs_size ||
        b > std::numeric_limits<std::uintptr_t>::max() - rhs_size) return true;
    return a < b + rhs_size && b < a + lhs_size;
}

} // namespace

ContractError make_gemm_dispatch(const GemmParams& p,
                                 const BufferView& a,
                                 const BufferView& b,
                                 const BufferView& c,
                                 const BufferView& out,
                                 GemmDispatch* dispatch) {
    if (!dispatch) return ContractError::null_dispatch;
    if (p.m == 0 || p.n == 0) return ContractError::zero_output_dimension;
    if (!std::isfinite(p.alpha) || !std::isfinite(p.beta))
        return ContractError::invalid_scalar;

    std::size_t mn = 0, mk = 0, kn = 0;
    std::size_t a_bytes = 0, b_bytes = 0, c_bytes = 0;
    if (!checked_mul(p.m, p.n, &mn) ||
        !checked_mul(p.m, p.k, &mk) ||
        !checked_mul(p.k, p.n, &kn) ||
        !checked_mul(mn, sizeof(float), &c_bytes) ||
        !checked_mul(mk, sizeof(float), &a_bytes) ||
        !checked_mul(kn, sizeof(float), &b_bytes)) {
        return ContractError::size_overflow;
    }
    // The shader uses 32-bit uint arithmetic for linear indexing and K-loop
    // increments. Reject dimensions that could wrap before a GPU dispatch.
    const auto shader_index_limit =
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max());
    const auto tile_edge_limit =
        std::numeric_limits<std::uint32_t>::max() - 15u;
    if (mn > shader_index_limit || mk > shader_index_limit ||
        kn > shader_index_limit || p.m > tile_edge_limit ||
        p.n > tile_edge_limit || p.k > tile_edge_limit) {
        return ContractError::shader_index_overflow;
    }

    if (!a.data || !b.data || !c.data || !out.data)
        return ContractError::null_buffer;
    if (a.size_bytes < a_bytes || b.size_bytes < b_bytes ||
        c.size_bytes < c_bytes || out.size_bytes < c_bytes)
        return ContractError::buffer_too_small;

    if (ranges_overlap(out.data, c_bytes, a.data, a_bytes) ||
        ranges_overlap(out.data, c_bytes, b.data, b_bytes) ||
        ranges_overlap(out.data, c_bytes, c.data, c_bytes))
        return ContractError::output_aliases_input;

    dispatch->params = p;
    dispatch->groups_x = p.n / 16u + (p.n % 16u != 0u);
    dispatch->groups_y = p.m / 16u + (p.m % 16u != 0u);
    dispatch->a_required_bytes = a_bytes;
    dispatch->b_required_bytes = b_bytes;
    dispatch->c_required_bytes = c_bytes;
    dispatch->out_required_bytes = c_bytes;
    return ContractError::none;
}

const char* contract_error_string(ContractError error) noexcept {
    switch (error) {
    case ContractError::none: return "none";
    case ContractError::null_dispatch: return "null dispatch output";
    case ContractError::zero_output_dimension: return "zero output dimension";
    case ContractError::shader_index_overflow: return "shader 32-bit index overflow";
    case ContractError::size_overflow: return "buffer size overflow";
    case ContractError::null_buffer: return "null buffer";
    case ContractError::buffer_too_small: return "buffer too small";
    case ContractError::output_aliases_input: return "output aliases input";
    case ContractError::invalid_scalar: return "alpha/beta must be finite";
    }
    return "unknown contract error";
}

} // namespace pne::gcn8
