#include "gnm_runtime_resolver.h"

#include <cassert>
#include <cstring>

using namespace pne::gcn8::runtime;

namespace {
uint32_t dispatch_stub(uint32_t*, uint32_t) { return 1; }
void flush_stub() {}
int32_t submit_stub(uint32_t, void* const[], uint32_t*, void* const[], uint32_t*) { return 0; }
int32_t done_stub() { return 0; }

struct LookupState {
    int missing_index = -1;
    int calls = 0;
};

bool fake_lookup(void* opaque, const char* name, void** out) {
    auto& state = *static_cast<LookupState*>(opaque);
    const char* names[] = {
        "sceGnmDispatchInitDefaultHardwareState",
        "sceGnmFlushGarlic",
        "sceGnmSubmitCommandBuffers",
        "sceGnmSubmitDone",
    };
    void* addresses[] = {
        reinterpret_cast<void*>(&dispatch_stub),
        reinterpret_cast<void*>(&flush_stub),
        reinterpret_cast<void*>(&submit_stub),
        reinterpret_cast<void*>(&done_stub),
    };
    for (int i = 0; i < 4; ++i) {
        if (std::strcmp(name, names[i]) == 0) {
            const int call = state.calls++;
            if (i == state.missing_index || call == state.missing_index) {
                *out = nullptr;
                return false;
            }
            *out = addresses[i];
            return true;
        }
    }
    return false;
}
}

int main() {
    GnmRuntimeFunctions functions{};
    LookupState success{};
    assert(resolve_gnm_runtime_functions(fake_lookup, &success, &functions));
    assert(functions.complete());
    assert(functions.dispatch_init_default_hardware_state == &dispatch_stub);
    assert(functions.flush_garlic == &flush_stub);
    assert(functions.submit_command_buffers == &submit_stub);
    assert(functions.submit_done == &done_stub);

    for (int missing = 0; missing < 4; ++missing) {
        LookupState failure{missing, 0};
        assert(!resolve_gnm_runtime_functions(fake_lookup, &failure, &functions));
        assert(!functions.complete());
        assert(functions.dispatch_init_default_hardware_state == nullptr);
        assert(functions.flush_garlic == nullptr);
        assert(functions.submit_command_buffers == nullptr);
        assert(functions.submit_done == nullptr);
    }

    assert(!resolve_gnm_runtime_functions(nullptr, nullptr, &functions));
    assert(!functions.complete());
    assert(!resolve_gnm_runtime_functions(fake_lookup, nullptr, nullptr));
    return 0;
}
