#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pne::gcn8::runtime {

enum class ProviderCensusStatus : uint8_t {
    complete,
    invalid_argument,
    list_failed,
    capacity_exceeded,
    module_info_failed
};

struct ProviderCensus {
    ProviderCensusStatus status = ProviderCensusStatus::invalid_argument;
    uint32_t normal_handle = 0;
    uint32_t neo_handle = 0;
    bool has_normal = false;
    bool has_neo = false;
};

using ModuleListFn = bool (*)(void*, uint32_t*, size_t, size_t*);
using ModuleNameFn = bool (*)(void*, uint32_t, char*, size_t);

// Pure policy layer around caller-supplied, passive OS queries. It never
// loads modules or resolves/calls driver exports.
inline ProviderCensus census_loaded_gnm_providers(
    ModuleListFn list_modules, ModuleNameFn get_module_name,
    void* context, uint32_t* handles, size_t capacity) noexcept {
    ProviderCensus result{};
    if (!list_modules || !get_module_name || !handles || capacity == 0)
        return result;

    size_t count = 0;
    if (!list_modules(context, handles, capacity, &count)) {
        result.status = ProviderCensusStatus::list_failed;
        return result;
    }
    if (count > capacity) {
        result.status = ProviderCensusStatus::capacity_exceeded;
        return result;
    }

    for (size_t i = 0; i < count; ++i) {
        char name[256]{};
        if (!get_module_name(context, handles[i], name, sizeof(name))) {
            result.status = ProviderCensusStatus::module_info_failed;
            return result;
        }
        if (std::strcmp(name, "libSceGnmDriver") == 0) {
            result.normal_handle = handles[i];
            result.has_normal = true;
        } else if (std::strcmp(name, "libSceGnmDriverForNeoMode") == 0) {
            result.neo_handle = handles[i];
            result.has_neo = true;
        }
    }

    result.status = ProviderCensusStatus::complete;
    return result;
}

} // namespace pne::gcn8::runtime
