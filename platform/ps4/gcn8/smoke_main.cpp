#include "compute_smoke.h"

#include <cstdio>

#ifndef PNE_GCN8_SMOKE_RUN_ID
#define PNE_GCN8_SMOKE_RUN_ID "manual"
#endif

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    char log_path[160]{};
    std::snprintf(log_path, sizeof(log_path),
                  "/data/pne_gcn8_compute_smoke_%s.log",
                  PNE_GCN8_SMOKE_RUN_ID);
    const int result = pne_gcn8_compute_smoke_run(
        "app0:/gcn8/compute_smoke_add.sb", log_path);
    std::printf("PNE_GCN8_SMOKE_EXIT=%d\n", result);
    std::fflush(stdout);
    return result;
}
