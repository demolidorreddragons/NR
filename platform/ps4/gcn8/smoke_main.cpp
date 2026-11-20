#include "compute_smoke.h"

#include <cstdio>
#include <cstring>

#ifndef PNE_GCN8_SMOKE_RUN_ID
#define PNE_GCN8_SMOKE_RUN_ID "manual"
#endif

static bool select_log_path(char* out, size_t capacity) {
    const char* roots[] = {"/user/data", "/data"};
    for (const char* root : roots) {
        std::snprintf(out, capacity, "%s/pne_gcn8_compute_smoke_%s.log",
                      root, PNE_GCN8_SMOKE_RUN_ID);
        FILE* f = std::fopen(out, "ab");
        if (f) {
            std::fclose(f);
            return true;
        }
    }
    out[0] = '\0';
    return false;
}

static void startup_log(const char* path, const char* message) {
    FILE* f = path && path[0] ? std::fopen(path, "ab") : nullptr;
    if (f) {
        std::fprintf(f, "%s\n", message);
        std::fflush(f);
        std::fclose(f);
    }
    std::printf("%s\n", message);
    std::fflush(stdout);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    char log_path[160]{};
    const bool persistent_log = select_log_path(log_path, sizeof(log_path));
    startup_log(persistent_log ? log_path : nullptr,
                persistent_log ? "PNE_GCN8_LOG_OPEN=1" : "PNE_GCN8_LOG_OPEN=0");
    if (persistent_log) {
        char path_line[220]{};
        std::snprintf(path_line, sizeof(path_line), "LOG_PATH=%s", log_path);
        startup_log(log_path, path_line);
    }
    startup_log(persistent_log ? log_path : nullptr, "PNE_GCN8_APP_MAIN_ENTERED");
    startup_log(persistent_log ? log_path : nullptr, "STAGE=BEFORE_COMPUTE_SMOKE");
#ifdef PNE_GCN8_STARTUP_ONLY
    const int result = 0;
    startup_log(persistent_log ? log_path : nullptr,
                "RESULT=PASS; MODE=STARTUP_ONLY; GNM_NOT_LINKED");
#else
    const int result = pne_gcn8_compute_smoke_run(
        "app0:/compute_smoke_add.sb", persistent_log ? log_path : nullptr);
#endif
    char exit_line[64]{};
    std::snprintf(exit_line, sizeof(exit_line), "PNE_GCN8_SMOKE_EXIT=%d", result);
    startup_log(persistent_log ? log_path : nullptr, exit_line);
    return result;
}
