/*
 * Neural Rendering package preflight entry point.
 *
 * This executable validates its bundled release manifest/PRX and performs a
 * process-local GNM module/export audit. It resolves addresses only: it never
 * invokes GNM, dispatches GPU work, installs/activates the Neural Rendering
 * PRX, edits GoldHEN configuration, or changes persistent system configuration.
 */
#include <Common.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <orbis/libkernel.h>
#include "release_manifest.h"
#include "sha256.h"

#define SOURCE_PRX "/app0/ps4_neural_engine.prx"
#define RELEASE_MANIFEST "/app0/pne-release.ini"
#define GNM_EXPORT_COUNT 5u
#define GCN8_ASSET_COUNT 6u

typedef struct {
    const char* path;
    const char* sha256;
} Gcn8Asset;

static const Gcn8Asset gcn8_assets[GCN8_ASSET_COUNT] = {
    { "/app0/gcn8/compute_smoke_add.gcn.bin", "bd6deda8f6fc7cc04f78b441864a0b57d63f259f8d048d6ac09747f0cc6988e5" },
    { "/app0/gcn8/e4m3_decode.gcn.bin", "08ed8e40dbab45038312f3d4db49a2dc49f209e7b393bc2c497a4b42ccb70c60" },
    { "/app0/gcn8/e4m3_encode.gcn.bin", "86b8c0a49fc46d513a27f344b1515cc7ab38353878215b29238d27d74e4d0142" },
    { "/app0/gcn8/gemm_fp16_accum.gcn.bin", "5bf7129019cab1e40a67b572dba41167c3e5346bfe3360ded939f84894008048" },
    { "/app0/gcn8/gemm_fp32.gcn.bin", "ca0325130b9b77294a3fd7c32957d076f90741584901570a2c1be5521a099412" },
    { "/app0/gcn8/gemm_fp32_wave64_tiled.gcn.bin", "5dd8cba2710502d5c1f930e4e0ddbe779a56b168900f1040c0e95b28c0f3f8bc" }
};

static const char* const gnm_exports[GNM_EXPORT_COUNT] = {
    "sceGnmSubmitDone",
    "sceGnmDispatchInitDefaultHardwareState",
    "sceGnmFlushGarlic",
    "sceGnmSubmitCommandBuffers",
    "sceGnmSubmitAndFlipCommandBuffers"
};

static void notify_preflight(const char* message) {
    OrbisNotificationRequest request;
    memset(&request, 0, sizeof(request));
    request.type = NotificationRequest;
    request.useIconImageUri = 1;
    request.targetId = -1;
    strncpy(request.message, message, sizeof(request.message) - 1);
    strncpy(request.iconUri, "cxml://psnotification/tex_icon_system",
            sizeof(request.iconUri) - 1);
    (void)sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
}


/* Verify the exact GFX8 code objects bundled from the pinned shader build.
 * This is file integrity only: no shader is submitted to the GPU. */
static unsigned audit_gcn8_assets(const char** invalid_asset) {
    unsigned verified = 0;
    if (invalid_asset) *invalid_asset = NULL;
    for (unsigned i = 0; i < GCN8_ASSET_COUNT; ++i) {
        char actual[65];
        if (pne_sha256_file(gcn8_assets[i].path, actual) &&
            strcmp(actual, gcn8_assets[i].sha256) == 0) {
            ++verified;
        } else if (invalid_asset && !*invalid_asset) {
            *invalid_asset = gcn8_assets[i].path;
        }
    }
    return verified;
}

/* Loads the system GNM module into this app process only; never calls GNM. */
static int32_t find_or_load_gnm_module(void) {
    OrbisKernelModule modules[256];
    size_t count = 0;
    if (sceKernelGetModuleList(modules, 256, &count) == 0) {
        const size_t limit = count < 256 ? count : 256;
        for (size_t i = 0; i < limit; ++i) {
            OrbisKernelModuleInfo info;
            memset(&info, 0, sizeof(info));
            info.size = sizeof(info);
            if (sceKernelGetModuleInfo(modules[i], &info) == 0 &&
                strstr(info.name, "libSceGnmDriver") != NULL)
                return (int32_t)modules[i];
        }
    }

    const char* sandbox_word = sceKernelGetFsSandboxRandomWord();
    if (!sandbox_word || !sandbox_word[0]) return -1;

    char module_path[192];
    const int written = snprintf(module_path, sizeof(module_path),
        "/%s/common/lib/libSceGnmDriver.sprx", sandbox_word);
    if (written <= 0 || (size_t)written >= sizeof(module_path)) return -1;

    const uint32_t raw_handle = (uint32_t)sceKernelLoadStartModule(
        module_path, 0, NULL, 0, NULL, NULL);
    const int32_t handle = (int32_t)raw_handle;
    return handle >= 0 ? handle : -1;
}

/* DLSYM is used only to verify symbol presence; returned addresses are unused. */
static unsigned audit_gnm_exports(int32_t module, const char** missing) {
    unsigned resolved = 0;
    if (missing) *missing = NULL;
    for (unsigned i = 0; i < GNM_EXPORT_COUNT; ++i) {
        void* address = NULL;
        const int32_t rc = sceKernelDlsym(module, gnm_exports[i], &address);
        if (rc == 0 && address != NULL) {
            ++resolved;
        } else if (missing && !*missing) {
            *missing = gnm_exports[i];
        }
    }
    return resolved;
}

int main(void) {
    char error[256] = {0};
    if (!pne_validate_release_manifest(RELEASE_MANIFEST, SOURCE_PRX,
                                       error, sizeof(error))) {
        notify_preflight("PNE preflight failed: package/PRX integrity check.");
        (void)sceKernelUsleep(3000000);
        return 1;
    }

    const char* invalid_asset = NULL;
    const unsigned assets_verified = audit_gcn8_assets(&invalid_asset);
    if (assets_verified != GCN8_ASSET_COUNT) {
        char message[256];
        (void)snprintf(message, sizeof(message),
            "PNE preflight FAIL: GCN8 assets %u/6; invalid %s; no GNM calls; NR PRX NOT activated.",
            assets_verified, invalid_asset ? invalid_asset : "unknown");
        notify_preflight(message);
        (void)sceKernelUsleep(4000000);
        return 4;
    }

    const int32_t gnm_module = find_or_load_gnm_module();
    if (gnm_module < 0) {
        notify_preflight("PNE preflight FAIL: GNM module unavailable; NR PRX NOT activated.");
        (void)sceKernelUsleep(4000000);
        return 2;
    }

    const char* missing_export = NULL;
    const unsigned resolved = audit_gnm_exports(gnm_module, &missing_export);
    if (resolved != GNM_EXPORT_COUNT) {
        char message[256];
        (void)snprintf(message, sizeof(message),
            "PNE preflight FAIL: GNM exports %u/5; missing %s; NR PRX NOT activated.",
            resolved, missing_export ? missing_export : "unknown");
        notify_preflight(message);
        (void)sceKernelUsleep(4000000);
        return 3;
    }

    notify_preflight("PNE preflight PASS. GCN8 assets 6/6; GNM exports 5/5; no GNM calls; NR PRX NOT activated.");
    (void)sceKernelUsleep(4000000);
    return 0;
}
