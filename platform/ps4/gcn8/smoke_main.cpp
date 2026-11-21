#include "compute_smoke.h"
#include "stage_log.h"

#include <cstdio>
#include <cstring>

#ifndef PNE_GCN8_LOG_UDP_PORT
#define PNE_GCN8_LOG_UDP_PORT 9999
#endif

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
    const char* udp_ip = "";
#ifdef PNE_GCN8_LOG_UDP_IP
    udp_ip = PNE_GCN8_LOG_UDP_IP;
#endif
    const int logger_status = stage_log_init(persistent_log ? log_path : nullptr,
                                             udp_ip, PNE_GCN8_LOG_UDP_PORT);
    if ((logger_status & STAGE_LOG_FILE_OK) == 0)
        startup_log(nullptr, "STAGE_LOG_FILE=UNAVAILABLE");
#ifdef STAGE_LOG_UDP
    stage_log((logger_status & STAGE_LOG_UDP_OK) ? "UDP_TRANSPORT_READY" : "UDP_TRANSPORT_UNAVAILABLE");
#endif
    stage_log("MAIN_ENTERED");
    stage_log(stage_log_install_signals() == 0 ? "SIGNALS_INSTALLED" : "SIGNAL_INSTALL_FAILED");
    stage_log("BEFORE_COMPUTE_SMOKE");
#ifdef PNE_GCN8_STARTUP_ONLY
    const int result = 0;
    stage_log("RESULT_PASS_STARTUP_ONLY_GNM_NOT_LINKED");
#elif defined(PNE_GCN8_GNM_LINK_ONLY)
    const int result = 0;
    stage_log("GNM_LINK_ONLY_MAIN_REACHED");
    stage_log("RESULT_PASS_GNM_LINK_ONLY_NO_GNM_CALLS");
#else
    const int result = pne_gcn8_compute_smoke_run(
        "app0:/compute_smoke_add.sb", persistent_log ? log_path : nullptr);
#endif
    char exit_line[64]{};
    std::snprintf(exit_line, sizeof(exit_line), "PNE_GCN8_SMOKE_EXIT=%d", result);
    stage_log(exit_line);
    startup_log(nullptr, exit_line);
    return result;
}
