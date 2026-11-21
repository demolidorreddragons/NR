#include "gnm_runtime_abi.h"

#include <type_traits>

using namespace pne::gcn8::runtime;

static_assert(std::is_pointer_v<DispatchInitDefaultHardwareStateFn>);
static_assert(std::is_pointer_v<FlushGarlicFn>);
static_assert(std::is_pointer_v<SubmitCommandBuffersFn>);
static_assert(std::is_pointer_v<SubmitDoneFn>);

static_assert(std::is_same_v<
    DispatchInitDefaultHardwareStateFn,
    uint32_t (*)(uint32_t*, uint32_t)>);
static_assert(std::is_same_v<FlushGarlicFn, void (*)(void)>);
static_assert(std::is_same_v<
    SubmitCommandBuffersFn,
    int32_t (*)(uint32_t, void* const[], uint32_t*, void* const[], uint32_t*)>);
static_assert(std::is_same_v<SubmitDoneFn, int32_t (*)(void)>);

int main() { return 0; }
