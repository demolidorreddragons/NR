#pragma once

#include "gnm_runtime_abi.h"

namespace pne::gcn8::runtime {

using SymbolLookup = bool (*)(void* context, const char* name, void** address);
using ProviderSymbolLookup = bool (*)(void* context, uint32_t provider_handle, const char* name, void** address);

struct GnmRuntimeFunctions {
    DispatchInitDefaultHardwareStateFn dispatch_init_default_hardware_state = nullptr;
    FlushGarlicFn flush_garlic = nullptr;
    SubmitCommandBuffersFn submit_command_buffers = nullptr;
    SubmitDoneFn submit_done = nullptr;

    bool complete() const noexcept {
        return dispatch_init_default_hardware_state != nullptr &&
               flush_garlic != nullptr &&
               submit_command_buffers != nullptr &&
               submit_done != nullptr;
    }

    void clear() noexcept {
        dispatch_init_default_hardware_state = nullptr;
        flush_garlic = nullptr;
        submit_command_buffers = nullptr;
        submit_done = nullptr;
    }
};


struct ProviderLookupAdapter {
    ProviderSymbolLookup lookup;
    void* context;
    uint32_t provider_handle;
};

inline bool lookup_selected_provider(void* opaque, const char* name,
                                     void** address) noexcept {
    auto* adapter = static_cast<ProviderLookupAdapter*>(opaque);
    return adapter && adapter->lookup &&
           adapter->lookup(adapter->context, adapter->provider_handle,
                           name, address);
}

// Resolves all required entry points atomically from the caller's module.
// No function is invoked here. On any missing/invalid symbol, the output
// table is cleared so callers cannot accidentally use a partial ABI.
inline bool resolve_gnm_runtime_functions(
    SymbolLookup lookup, void* context, GnmRuntimeFunctions* output) noexcept {
    if (!output) return false;
    output->clear();
    if (!lookup) return false;

    void* dispatch = nullptr;
    void* flush = nullptr;
    void* submit = nullptr;
    void* done = nullptr;
    if (!lookup(context, "sceGnmDispatchInitDefaultHardwareState", &dispatch) ||
        !dispatch ||
        !lookup(context, "sceGnmFlushGarlic", &flush) || !flush ||
        !lookup(context, "sceGnmSubmitCommandBuffers", &submit) || !submit ||
        !lookup(context, "sceGnmSubmitDone", &done) || !done) {
        return false;
    }

    output->dispatch_init_default_hardware_state =
        reinterpret_cast<DispatchInitDefaultHardwareStateFn>(dispatch);
    output->flush_garlic = reinterpret_cast<FlushGarlicFn>(flush);
    output->submit_command_buffers =
        reinterpret_cast<SubmitCommandBuffersFn>(submit);
    output->submit_done = reinterpret_cast<SubmitDoneFn>(done);
    return output->complete();
}

inline bool resolve_gnm_runtime_functions_from_provider(
    ProviderSymbolLookup lookup, void* context, uint32_t provider_handle,
    GnmRuntimeFunctions* output) noexcept {
    if (!output) return false;
    output->clear();
    if (!lookup) return false;
    ProviderLookupAdapter adapter{lookup, context, provider_handle};
    return resolve_gnm_runtime_functions(
        lookup_selected_provider, &adapter, output);
}

} // namespace pne::gcn8::runtime
