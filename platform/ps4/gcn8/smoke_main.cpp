#include "compute_smoke.h"

#include <cstdio>
#include <cstring>

#ifndef PNE_GCN8_SMOKE_RUN_ID
#define PNE_GCN8_SMOKE_RUN_ID "manual"
#endif

static void startup_log(const char* path, const char* message) {
    FILE* f = std::fopen(path, "ab");
    if (f) {
        std::fprintf(f, "%s\\n", message);
        std::fflush(f);
        std::fclose(f);
    }
    std::printf("%s\\n", message);
    std::fflush(stdout);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    char log_path[160]{};
    std::snprintf(log_path, sizeof(log_path),
                  "/data/pne_gcn8_compute_smoke_%s.log",
                  PNE_GCN8_SMOKE_RUN_ID);
    startup_log(log_path, "PNE_GCN8_APP_MAIN_ENTERED");
    startup_log(log_path, "STAGE=BEFORE_COMPUTE_SMOKE");
    const int result = pne_gcn8_compute_smoke_run(
        "app0:/compute_smoke_add.sb", log_path);
    char exit_line[64]{};
    std::snprintf(exit_line, sizeof(exit_line), "PNE_GCN8_SMOKE_EXIT=%d", result);
    startup_log(log_path, exit_line);
    return result;
}
