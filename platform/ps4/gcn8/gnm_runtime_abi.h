#pragma once

#include <cstdint>

/*
 * ABI contracts copied from PS4-OpenGNM/opengnm at
 * 4b295ca54c82c83acf308d1c646a2dfa9ae57350:
 *   include/gnmdriver.h and src/driver_orbis.c
 *
 * Do not derive these signatures from OpenOrbis GnmDriver.h: that header
 * exposes several incomplete void(void) stubs.
 */
#if defined(__x86_64__)
#define PNE_GNM_SYSV_ABI __attribute__((sysv_abi))
#else
#define PNE_GNM_SYSV_ABI
#endif

namespace pne::gcn8::runtime {

using DispatchInitDefaultHardwareStateFn =
    uint32_t (PNE_GNM_SYSV_ABI *)(uint32_t* command_buffer, uint32_t size_dwords);

using FlushGarlicFn = void (PNE_GNM_SYSV_ABI *)(void);

using SubmitCommandBuffersFn =
    int32_t (PNE_GNM_SYSV_ABI *)(
        uint32_t count,
        void* const dcb_gpu_addresses[],
        uint32_t* dcb_sizes_bytes,
        void* const ccb_gpu_addresses[],
        uint32_t* ccb_sizes_bytes);

using SubmitDoneFn = int32_t (PNE_GNM_SYSV_ABI *)(void);

} // namespace pne::gcn8::runtime
