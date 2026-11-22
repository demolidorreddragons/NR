/*
 * Neural Rendering package preflight entry point.
 *
 * This executable is intentionally read-only. Installing/running the PKG must
 * not copy or activate the GoldHEN PRX, edit the GoldHEN plugin configuration, restart GoldHEN, or
 * alter existing console state. PRX deployment/activation is a separate,
 * explicitly authorized operation.
 */
#include <Common.h>
#include <string.h>
#include <orbis/libkernel.h>
#include "release_manifest.h"

#define SOURCE_PRX "/app0/ps4_neural_engine.prx"
#define RELEASE_MANIFEST "/app0/pne-release.ini"

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

int main(void) {
    char error[256] = {0};

    if (!pne_validate_release_manifest(RELEASE_MANIFEST, SOURCE_PRX,
                                       error, sizeof(error))) {
        notify_preflight("PNE preflight failed: package/PRX integrity check.");
        (void)sceKernelUsleep(3000000);
        return 1;
    }

    notify_preflight("PNE preflight PASS. PRX NOT installed or activated; no system files changed.");
    (void)sceKernelUsleep(3000000);
    return 0;
}
