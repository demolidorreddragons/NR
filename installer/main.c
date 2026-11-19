#include <Common.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <orbis/libkernel.h>

#define SOURCE_PRX "/app0/ps4_neural_engine.prx"
#define DEST_DIR "/data/GoldHEN/plugins"
#define DEST_PRX DEST_DIR "/ps4_neural_engine.prx"
#define DEST_TMP DEST_PRX ".pne-tmp"
#define CONFIG "/data/GoldHEN/plugins.ini"
#define CONFIG_TMP CONFIG ".pne-tmp"
#define STATUS "/data/GoldHEN/ps4_neural_engine_install.log"
#define MAX_CONFIG 65536

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

static void write_status(const char* message) {
    FILE* f = fopen(STATUS, "wb");
    if (!f) return;
    fprintf(f, "PS4 Neural Engine installer\n%s\n", message);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
}

static int copy_prx(char* error, size_t error_size) {
    struct stat src_stat, dst_stat;
    if (stat(SOURCE_PRX, &src_stat) != 0) {
        snprintf(error, error_size, "ERROR: cannot stat %s (errno=%d).", SOURCE_PRX, errno);
        return 0;
    }
    if (src_stat.st_size <= 0) {
        snprintf(error, error_size, "ERROR: source PRX is empty (%s).", SOURCE_PRX);
        return 0;
    }

    FILE* in = fopen(SOURCE_PRX, "rb");
    if (!in) {
        snprintf(error, error_size, "ERROR: cannot open source PRX (errno=%d).", errno);
        return 0;
    }
    FILE* out = fopen(DEST_TMP, "wb");
    if (!out) {
        int saved_errno = errno;
        fclose(in);
        snprintf(error, error_size, "ERROR: cannot create temporary PRX in %s (errno=%d).", DEST_DIR, saved_errno);
        return 0;
    }

    char buffer[16384];
    size_t n;
    int ok = 1;
    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, n, out) != n) { ok = 0; break; }
    }
    if (ferror(in)) ok = 0;
    if (fflush(out) != 0 || fsync(fileno(out)) != 0) ok = 0;
    if (fclose(in) != 0) ok = 0;
    if (fclose(out) != 0) ok = 0;

    if (!ok) {
        int saved_errno = errno;
        remove(DEST_TMP);
        snprintf(error, error_size, "ERROR: PRX copy/write failed (errno=%d).", saved_errno);
        return 0;
    }
    if (stat(DEST_TMP, &dst_stat) != 0) {
        int saved_errno = errno;
        remove(DEST_TMP);
        snprintf(error, error_size, "ERROR: cannot verify copied PRX (errno=%d).", saved_errno);
        return 0;
    }
    if (dst_stat.st_size != src_stat.st_size) {
        remove(DEST_TMP);
        snprintf(error, error_size, "ERROR: PRX size mismatch (source=%lld copied=%lld).",
                 (long long)src_stat.st_size, (long long)dst_stat.st_size);
        return 0;
    }
    if (rename(DEST_TMP, DEST_PRX) != 0) {
        int saved_errno = errno;
        remove(DEST_TMP);
        snprintf(error, error_size, "ERROR: cannot activate copied PRX (errno=%d).", saved_errno);
        return 0;
    }
    return 1;
}

static void trim_line(char* line) {
    size_t n = strlen(line);
    while (n && (line[n-1] == '\\r' || line[n-1] == '\\n' ||
                 isspace((unsigned char)line[n-1]))) line[--n] = 0;
    char* p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != line) memmove(line, p, strlen(p) + 1);
}

static int configure_loader(char* error, size_t error_size) {
    char input[MAX_CONFIG];
    char output[MAX_CONFIG + 256];
    size_t length = 0, used = 0;
    FILE* in = fopen(CONFIG, "rb");
    if (in) {
        length = fread(input, 1, sizeof(input)-1, in);
        if (ferror(in) || !feof(in)) {
            fclose(in);
            snprintf(error, error_size, "ERROR: plugins.ini is too large or unreadable.");
            return 0;
        }
        fclose(in);
    } else if (errno != ENOENT) {
        snprintf(error, error_size, "ERROR: cannot read existing plugins.ini (errno=%d); preserving it.", errno);
        return 0;
    }
    input[length] = 0;

    int in_default = 0, found_default = 0, found_entry = 0;
    size_t pos = 0;
    while (pos < length) {
        size_t start = pos;
        while (pos < length && input[pos] != '\\n') pos++;
        if (pos < length) pos++;
        size_t line_len = pos - start;
        char line[512];
        size_t copy_len = line_len < sizeof(line)-1 ? line_len : sizeof(line)-1;
        memcpy(line, input + start, copy_len);
        line[copy_len] = 0;
        trim_line(line);
        if (line[0] == '[') {
            in_default = strcmp(line, "[default]") == 0;
            if (in_default) found_default = 1;
        }
        if (in_default && strcmp(line, DEST_PRX) == 0) found_entry = 1;
    }
    if (found_entry) return 1;

    in_default = 0;
    pos = 0;
    while (pos < length) {
        size_t start = pos;
        while (pos < length && input[pos] != '\\n') pos++;
        if (pos < length) pos++;
        size_t line_len = pos - start;
        char line[512];
        size_t copy_len = line_len < sizeof(line)-1 ? line_len : sizeof(line)-1;
        memcpy(line, input + start, copy_len);
        line[copy_len] = 0;
        trim_line(line);

        if (line[0] == '[') in_default = strcmp(line, "[default]") == 0;
        if (used + line_len + 1 >= sizeof(output)) {
            snprintf(error, error_size, "ERROR: plugins.ini exceeds safe output size.");
            return 0;
        }
        memcpy(output + used, input + start, line_len);
        used += line_len;

        if (in_default && strcmp(line, "[default]") == 0) {
            if (used && output[used-1] != '\\n') output[used++] = '\\n';
            int n = snprintf(output + used, sizeof(output)-used, "%s\\n", DEST_PRX);
            if (n < 0 || (size_t)n >= sizeof(output)-used) {
                snprintf(error, error_size, "ERROR: no room to add Neural Engine to plugins.ini.");
                return 0;
            }
            used += (size_t)n;
        }
    }

    if (!found_default) {
        if (used && output[used-1] != '\\n') output[used++] = '\\n';
        const char* suffix = "\\n[default]\\n" DEST_PRX "\\n";
        size_t n = strlen(suffix);
        if (used + n >= sizeof(output)) {
            snprintf(error, error_size, "ERROR: no room to create [default] in plugins.ini.");
            return 0;
        }
        memcpy(output + used, suffix, n);
        used += n;
    }

    FILE* out = fopen(CONFIG_TMP, "wb");
    if (!out) {
        snprintf(error, error_size, "ERROR: cannot create temporary plugins.ini (errno=%d).", errno);
        return 0;
    }
    int ok = fwrite(output, 1, used, out) == used;
    if (fflush(out) != 0 || fsync(fileno(out)) != 0) ok = 0;
    if (fclose(out) != 0) ok = 0;
    if (!ok || rename(CONFIG_TMP, CONFIG) != 0) {
        int saved_errno = errno;
        remove(CONFIG_TMP);
        snprintf(error, error_size, "ERROR: cannot save updated plugins.ini (errno=%d).", saved_errno);
        return 0;
    }
    return 1;
}

int main(void) {
    char error[256];
    if (mkdir("/data/GoldHEN", 0777) != 0 && errno != EEXIST) {
        snprintf(error, sizeof(error), "ERROR: cannot create /data/GoldHEN (errno=%d).", errno);
        write_status(error);
        notify("PNE: falha ao preparar pasta GoldHEN. Veja install.log via FTP.");
        sceKernelUsleep(3000000);
        return 1;
    }
    if (mkdir(DEST_DIR, 0777) != 0 && errno != EEXIST) {
        snprintf(error, sizeof(error), "ERROR: cannot create plugin directory (errno=%d).", errno);
        write_status(error);
        notify("PNE: falha ao criar pasta de plugins. Veja install.log via FTP.");
        sceKernelUsleep(3000000);
        return 1;
    }

    if (!copy_prx(error, sizeof(error))) {
        write_status(error);
        notify("PNE: falha ao copiar PRX. O log agora informa a etapa e errno.");
        sceKernelUsleep(3000000);
        return 2;
    }
    if (!configure_loader(error, sizeof(error))) {
        write_status(error);
        notify("PNE: PRX copiado, mas plugins.ini nao foi atualizado. Veja install.log.");
        sceKernelUsleep(3000000);
        return 3;
    }

    write_status("OK: PRX copied and size-verified; plugins.ini was updated without replacing existing entries. Restart GoldHEN or reboot to load.");
    notify("PNE instalado automaticamente. Reinicie o GoldHEN/PS4 para carregar.");
    sceKernelUsleep(3000000);
    return 0;
}
