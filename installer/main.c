#include <Common.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <orbis/libkernel.h>

#define SOURCE_PRX "/app0/ps4_neural_engine.prx"
#define DEST_DIR "/data/GoldHEN/plugins"
#define DEST_PRX DEST_DIR "/ps4_neural_engine.prx"
#define CONFIG "/data/GoldHEN/plugins.ini"
#define CONFIG_TMP "/data/GoldHEN/plugins.ini.pne-tmp"
#define CONFIG_LINE DEST_PRX

static void notify(const char* message) {
    OrbisNotificationRequest request;
    memset(&request, 0, sizeof(request));
    request.type = NotificationRequest;
    request.useIconImageUri = 1;
    request.targetId = -1;
    strncpy(request.message, message, sizeof(request.message)-1);
    strncpy(request.iconUri, "cxml://psnotification/tex_icon_system", sizeof(request.iconUri)-1);
    sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
}

static int copy_prx(void) {
    FILE* in = fopen(SOURCE_PRX, "rb");
    if (!in) return 0;
    FILE* out = fopen(DEST_PRX, "wb");
    if (!out) { fclose(in); return 0; }
    char buffer[16384];
    size_t n;
    int ok = 1;
    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, n, out) != n) { ok = 0; break; }
    }
    if (ferror(in)) ok = 0;
    if (fclose(in) != 0) ok = 0;
    if (fclose(out) != 0) ok = 0;
    return ok;
}

static int configure_loader(void) {
    char input[32768];
    size_t length = 0;
    FILE* in = fopen(CONFIG, "rb");
    if (in) {
        length = fread(input, 1, sizeof(input)-1, in);
        if (ferror(in) || !feof(in)) { fclose(in); return 0; }
        fclose(in);
    }
    input[length] = '\0';
    if (strstr(input, CONFIG_LINE)) return 1;

    FILE* out = fopen(CONFIG_TMP, "wb");
    if (!out) return 0;
    char* section = NULL;
    char* cursor = input;
    while (*cursor) {
        char* line_end = strchr(cursor, '\n');
        size_t line_len = line_end ? (size_t)(line_end - cursor + 1) : strlen(cursor);
        if ((line_len >= 9 && strncmp(cursor, "[default]", 9) == 0)) {
            section = cursor + line_len;
            break;
        }
        cursor += line_len;
    }
    int ok = 1;
    if (section) {
        size_t prefix = (size_t)(section - input);
        if (fwrite(input, 1, prefix, out) != prefix) ok = 0;
        if (ok && fprintf(out, CONFIG_LINE "\n") < 0) ok = 0;
        if (ok && fwrite(section, 1, length - prefix, out) != length - prefix) ok = 0;
    } else {
        if (length && input[length-1] != '\n' && fputc('\n', out) == EOF) ok = 0;
        if (ok && fprintf(out, "\n[default]\n" CONFIG_LINE "\n") < 0) ok = 0;
    }
    if (fclose(out) != 0) ok = 0;
    if (!ok || rename(CONFIG_TMP, CONFIG) != 0) {
        remove(CONFIG_TMP);
        return 0;
    }
    return 1;
}

int main(void) {
    mkdir("/data/GoldHEN", 0777);
    mkdir(DEST_DIR, 0777);
    if (!copy_prx()) {
        notify("PNE: failed to copy plugin; check GoldHEN permissions");
        sceKernelUsleep(2500000);
        return 1;
    }
    if (!configure_loader()) {
        notify("PNE: plugin copied; add its path to plugins.ini manually");
        sceKernelUsleep(3000000);
        return 2;
    }
    notify("PS4 Neural Engine diagnostics installed. Restart GoldHEN or reboot to load.");
    sceKernelUsleep(3000000);
    return 0;
}
