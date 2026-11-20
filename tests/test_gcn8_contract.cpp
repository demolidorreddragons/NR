#include "compute_contract.h"

#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

using namespace pne::gcn8;

int main() {
    std::vector<float> a(17 * 19), b(19 * 31), c(17 * 31), out(17 * 31);
    const BufferView av{a.data(), a.size() * sizeof(float)};
    const BufferView bv{b.data(), b.size() * sizeof(float)};
    const BufferView cv{c.data(), c.size() * sizeof(float)};
    const BufferView ov{out.data(), out.size() * sizeof(float)};
    GemmDispatch d{};

    GemmParams p{17, 31, 19, 1.0f, 0.0f};
    assert(make_gemm_dispatch(p, av, bv, cv, ov, &d) == ContractError::none);
    assert(d.groups_x == 2 && d.groups_y == 2);
    assert(d.a_required_bytes == 17u * 19u * sizeof(float));
    assert(d.b_required_bytes == 19u * 31u * sizeof(float));
    assert(d.out_required_bytes == 17u * 31u * sizeof(float));

    p.m = 0;
    assert(make_gemm_dispatch(p, av, bv, cv, ov, &d) == ContractError::zero_output_dimension);
    p = {17, 31, 19, 1.0f, 0.0f};
    BufferView short_a{a.data(), 4};
    assert(make_gemm_dispatch(p, short_a, bv, cv, ov, &d) == ContractError::buffer_too_small);
    BufferView alias_out{b.data(), b.size() * sizeof(float)};
    assert(make_gemm_dispatch(p, av, bv, cv, alias_out, &d) == ContractError::output_aliases_input);
    p.alpha = std::numeric_limits<float>::infinity();
    assert(make_gemm_dispatch(p, av, bv, cv, ov, &d) == ContractError::invalid_scalar);
    assert(make_gemm_dispatch({17, 31, 19, 1.0f, 0.0f}, av, bv, cv, ov, nullptr)
           == ContractError::null_buffer);
    assert(make_gemm_dispatch({UINT32_MAX, UINT32_MAX, 1, 1.0f, 0.0f},
                              av, bv, cv, ov, &d) == ContractError::size_overflow);
    return 0;
}
