#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int pne_telemetry_start(const char* root);
void pne_telemetry_stop(void);
void pne_telemetry_on_present(uint64_t ticks,uint64_t frequency,uint32_t title_id,uint32_t display_buffer);
// rgba must point to CPU-readable staging memory after GNM readback, fence wait and cache invalidation.
int pne_adapter_submit_readback(const uint8_t* rgba,uint32_t width,uint32_t height,size_t row_pitch,uint32_t frame_number);
#ifdef __cplusplus
}
#endif
