#include "plugin_common.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void NotifyStatic(const char* icon_uri, const char* text) {
    OrbisNotificationRequest request;
    memset(&request, 0, sizeof(request));
    request.type = NotificationRequest;
    request.useIconImageUri = 1;
    request.targetId = -1;
    strncpy(request.message, text, sizeof(request.message)-1);
    strncpy(request.iconUri, icon_uri, sizeof(request.iconUri)-1);
    sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
}

int NotifyStaticChecked(const char* icon_uri, const char* text) {
    if (!icon_uri || !text) return 0;
    OrbisNotificationRequest request;
    memset(&request, 0, sizeof(request));
    const size_t length = strlen(text);
    if (length >= sizeof(request.message)) return 0;
    request.type = NotificationRequest;
    request.useIconImageUri = 1;
    request.targetId = -1;
    memcpy(request.message, text, length + 1);
    strncpy(request.iconUri, icon_uri, sizeof(request.iconUri)-1);
    return sceKernelSendNotificationRequest(0, &request, sizeof(request), 0) >= 0;
}

void Notify(const char* icon_uri, const char* format, ...) {
    OrbisNotificationRequest request;
    memset(&request, 0, sizeof(request));
    va_list args;
    va_start(args, format);
    vsnprintf(request.message, sizeof(request.message), format, args);
    va_end(args);
    request.type = NotificationRequest;
    request.useIconImageUri = 1;
    request.targetId = -1;
    strncpy(request.iconUri, icon_uri, sizeof(request.iconUri)-1);
    sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
}
