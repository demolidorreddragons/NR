#include "compute_smoke.h"
#include "stage_log.h"
#ifdef PNE_GCN8_GNM_LINK_ONLY
#include "gnmdriver.h"
#endif
#ifdef PNE_GCN8_GNM_LOAD_ONLY
#include <orbis/libkernel.h>
#endif

#include <cstdio>
#include <cstring>
#include <cstdint>

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


#ifdef PNE_GCN8_GNM_LOAD_ONLY
static int run_gnm_load_probe(void) {
    OrbisKernelModule modules[256]{};
    size_t count=0;
    const int32_t list_rc=sceKernelGetModuleList(modules,256,&count);
    char line[256]{};
    std::snprintf(line,sizeof(line),"GNM_MODULE_LIST rc=0x%08x count=%zu",
                  static_cast<unsigned>(list_rc),count);
    stage_log(line);

    int32_t handle=-1;
    if (list_rc==0) {
        const size_t limit=count<256?count:256;
        for (size_t i=0;i<limit;++i) {
            OrbisKernelModuleInfo info{};
            info.size=sizeof(info);
            const int32_t info_rc=sceKernelGetModuleInfo(modules[i],&info);
            if (info_rc!=0) continue;
            std::snprintf(line,sizeof(line),"GNM_MODULE_ENTRY name=%s handle=0x%08x",
                          info.name,static_cast<unsigned>(modules[i]));
            stage_log(line);
            if (std::strstr(info.name,"libSceGnmDriver")!=nullptr)
                handle=static_cast<int32_t>(modules[i]);
        }
    }

    if (handle>=0) {
        std::snprintf(line,sizeof(line),"GNM_MODULE_ALREADY_LOADED handle=0x%08x",
                      static_cast<unsigned>(handle));
        stage_log(line);
    } else {
        const char* word=sceKernelGetFsSandboxRandomWord();
        std::snprintf(line,sizeof(line),"GNM_SANDBOX_WORD=%s",word?word:"(null)");
        stage_log(line);
        char sandbox_path[192]{};
        const bool have_sandbox_word=word && word[0];
        if (have_sandbox_word)
            std::snprintf(sandbox_path,sizeof(sandbox_path),
                          "/%s/common/lib/libSceGnmDriver.sprx",word);
        const char* paths[]={have_sandbox_word?sandbox_path:nullptr,
                             "libSceGnmDriver.sprx",
                             "/system/common/lib/libSceGnmDriver.sprx"};
        for (const char* candidate:paths) {
            if (!candidate) continue;
            const uint32_t raw=sceKernelLoadStartModule(candidate,0,nullptr,0,nullptr,nullptr);
            const int32_t loaded=static_cast<int32_t>(raw);
            std::snprintf(line,sizeof(line),"GNM_LOAD path=%s rc=0x%08x",
                          candidate,static_cast<unsigned>(raw));
            stage_log(line);
            if (loaded>=0) {
                handle=loaded;
                break;
            }
        }
    }

    if (handle>=0) {
        void* address=nullptr;
        const int32_t dlsym_rc=sceKernelDlsym(handle,"sceGnmSubmitDone",&address);
        std::snprintf(line,sizeof(line),
                      "GNM_DLSYM symbol=sceGnmSubmitDone rc=0x%08x address=0x%llx",
                      static_cast<unsigned>(dlsym_rc),
                      static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(address)));
        stage_log(line);
        stage_log(dlsym_rc==0 && address
                      ? "RESULT_PASS_GNM_DYNAMIC_LOAD_AND_DLSYM"
                      : "RESULT_FAIL_GNM_DLSYM");
    } else {
        stage_log("RESULT_FAIL_GNM_MODULE_LOAD");
    }

    /* A failed probe is a logged diagnostic result, not an abnormal process exit. */
    return 0;
}
#endif

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
#elif defined(PNE_GCN8_GNM_LOAD_ONLY)
    const int result = run_gnm_load_probe();
#elif defined(PNE_GCN8_GNM_LINK_ONLY)
    /* Taking the address forces a real import relocation without calling GNM. */
    const std::uintptr_t gnm_import_address =
        reinterpret_cast<std::uintptr_t>(&sceGnmSubmitDone);
    stage_log(gnm_import_address != 0 ? "GNM_IMPORT_RESOLVED" :
                                       "GNM_IMPORT_ADDRESS_NULL");
    const int result = gnm_import_address != 0 ? 0 : 1;
    stage_log("GNM_LINK_ONLY_MAIN_REACHED_NO_GNM_CALLS");
    stage_log(result == 0 ? "RESULT_PASS_GNM_LINK_ONLY" :
                            "RESULT_FAIL_GNM_IMPORT_ADDRESS");
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
