#include "gnm_provider_census.h"

#include <cassert>
#include <cstring>

using namespace pne::gcn8::runtime;

namespace {
struct Fake {
    uint32_t handles[4]{11, 22, 33, 44};
    const char* names[4]{
        "libSceGnmDriver", "libSceGnmDriverForNeoMode",
        "libSceGnmDriver.extra", "unrelated"};
    size_t count = 4;
    bool list_ok = true;
    int fail_info_index = -1;
};
bool list(void* p, uint32_t* out, size_t capacity, size_t* count) {
    auto& f = *static_cast<Fake*>(p);
    if (!f.list_ok) return false;
    *count = f.count;
    const size_t n = f.count < capacity ? f.count : capacity;
    for (size_t i = 0; i < n; ++i) out[i] = f.handles[i];
    return true;
}
bool name(void* p, uint32_t handle, char* out, size_t size) {
    auto& f = *static_cast<Fake*>(p);
    for (size_t i = 0; i < f.count && i < 4; ++i) {
        if (f.handles[i] == handle) {
            if (static_cast<int>(i) == f.fail_info_index) return false;
            std::strncpy(out, f.names[i], size - 1);
            return true;
        }
    }
    return false;
}
}

int main() {
    uint32_t handles[4]{};
    Fake f{};
    auto r = census_loaded_gnm_providers(list, name, &f, handles, 4);
    assert(r.status == ProviderCensusStatus::complete);
    assert(r.has_normal && r.normal_handle == 11);
    assert(r.has_neo && r.neo_handle == 22);

    uint32_t selected = 0;
    assert(select_gnm_provider_handle(r, &selected));
    assert(selected == 11); // normal provider wins when both exist

    f.count = 1;
    r = census_loaded_gnm_providers(list, name, &f, handles, 4);
    assert(r.status == ProviderCensusStatus::complete);
    assert(r.has_normal && !r.has_neo);
    assert(select_gnm_provider_handle(r, &selected));
    assert(selected == 11);

    f.count = 4;
    f.names[0] = "unrelated";
    f.names[1] = "libSceGnmDriverForNeoMode";
    r = census_loaded_gnm_providers(list, name, &f, handles, 4);
    assert(r.status == ProviderCensusStatus::complete);
    assert(!r.has_normal && r.has_neo);
    assert(select_gnm_provider_handle(r, &selected));
    assert(selected == 22);

    f.names[0] = "libSceGnmDriver";

    f.count = 4;
    r = census_loaded_gnm_providers(list, name, &f, handles, 3);
    assert(r.status == ProviderCensusStatus::capacity_exceeded);
    assert(!r.has_normal && !r.has_neo);
    assert(!select_gnm_provider_handle(r, &selected));

    f.list_ok = false;
    r = census_loaded_gnm_providers(list, name, &f, handles, 4);
    assert(r.status == ProviderCensusStatus::list_failed);

    f.list_ok = true;
    f.fail_info_index = 1;
    r = census_loaded_gnm_providers(list, name, &f, handles, 4);
    assert(r.status == ProviderCensusStatus::module_info_failed);

    r = census_loaded_gnm_providers(nullptr, name, &f, handles, 4);
    assert(r.status == ProviderCensusStatus::invalid_argument);
    return 0;
}
