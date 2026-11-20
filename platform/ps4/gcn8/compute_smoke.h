#ifndef PNE_GCN8_COMPUTE_SMOKE_H
#define PNE_GCN8_COMPUTE_SMOKE_H

#ifdef __cplusplus
extern "C" {
#endif

// Executes one 64-lane buffer->compute->buffer test. It is never called by
// the normal GoldHEN plugin entrypoint; only a dedicated test app may invoke it.
int pne_gcn8_compute_smoke_run(const char* shader_path, const char* log_path);

#ifdef __cplusplus
}
#endif
#endif
