/* Keep operation-specific errno details in the persistent install log for remote diagnosis. */
#include <Common.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
/* Runtime installer: POSIX file-copy path with byte-accurate verification. */
/* Bridge transfer uses the same content-addressed filename as the PS4 destination. */
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <orbis/libkernel.h>

/* Retry interrupted reads without advancing only one side of a byte comparison. */
static ssize_t read_retry(int fd, void* buffer, size_t capacity) {
    ssize_t n;
    do {
        n = read(fd, buffer, capacity);
    } while (n < 0 && errno == EINTR);
    return n;
}
/* Accumulate short reads so source and destination block sizes are comparable. */
static ssize_t read_block(int fd, void* buffer, size_t capacity) {
    size_t total = 0;
    while (total < capacity) {
        ssize_t n = read_retry(fd, (char*)buffer + total, capacity - total);
        if (n < 0) return -1;
        if (n == 0) break;
        total += (size_t)n;
    }
    return (ssize_t)total;
}


#define SOURCE_PRX "/app0/ps4_neural_engine.prx"
#define DEST_DIR "/data/GoldHEN/plugins"
#define DEST_PRX DEST_DIR "/ps4_neural_engine.prx"
#define DEST_TMP DEST_PRX ".pne-tmp"
#define DEST_BACKUP DEST_PRX ".pne-backup"
#define CONFIG "/data/GoldHEN/plugins.ini"
#define CONFIG_TMP CONFIG ".pne-tmp"
#define CONFIG_BACKUP CONFIG ".pne-backup"
#define STATUS "/data/GoldHEN/ps4_neural_engine_install.log"
#define MAX_CONFIG 65536

/* O_EXCL makes stale files and symlinks at staging paths a hard stop, never a target to truncate. */
static int open_staged_file(const char* path, int* fd_out) {
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return 0;
    *fd_out = fd;
    return 1;
}

static FILE* fopen_staged_file(const char* path) {
    int fd = -1;
    if (!open_staged_file(path, &fd)) return NULL;
    FILE* stream = fdopen(fd, "wb");
    if (!stream) {
        int saved_errno = errno;
        close(fd);
        remove(path);
        errno = saved_errno;
    }
    return stream;
}

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
    int in = open(SOURCE_PRX, O_RDONLY);
    if (in < 0) {
        snprintf(error, error_size, "ERROR: cannot open source PRX %s (errno=%d).", SOURCE_PRX, errno);
        return 0;
    }

    int out = -1;
    if (!open_staged_file(DEST_TMP, &out)) out = -1;
    if (out < 0) {
        int saved_errno = errno;
        close(in);
        snprintf(error, error_size, "ERROR: cannot create temporary PRX in %s (errno=%d).", DEST_DIR, saved_errno);
        return 0;
    }

    char buffer[16384];
    long long source_bytes = 0;
    long long written_bytes = 0;
    int ok = 1;
    int saved_errno = 0;
    for (;;) {
        ssize_t n = read_retry(in, buffer, sizeof(buffer));
        if (n == 0) break;
        if (n < 0) {
            ok = 0;
            saved_errno = errno;
            break;
        }
        source_bytes += n;
        ssize_t offset = 0;
        while (offset < n) {
            ssize_t written = write(out, buffer + offset, (size_t)(n - offset));
            if (written < 0) {
                if (errno == EINTR) continue;
                ok = 0;
                saved_errno = errno;
                break;
            }
            if (written == 0) {
                ok = 0;
                saved_errno = EIO;
                break;
            }
            offset += written;
            written_bytes += written;
        }
        if (!ok) break;
    }

    if (ok && fsync(out) != 0) {
        ok = 0;
        saved_errno = errno;
    }
    if (close(in) != 0 && ok) {
        ok = 0;
        saved_errno = errno;
    }
    if (close(out) != 0 && ok) {
        ok = 0;
        saved_errno = errno;
    }

    if (!ok) {
        remove(DEST_TMP);
        snprintf(error, error_size, "ERROR: PRX copy failed after reading %lld and writing %lld bytes (errno=%d).",
                 source_bytes, written_bytes, saved_errno);
        return 0;
    }
    if (source_bytes <= 0 || written_bytes != source_bytes) {
        remove(DEST_TMP);
        snprintf(error, error_size, "ERROR: PRX copy count mismatch (read=%lld written=%lld).",
                 source_bytes, written_bytes);
        return 0;
    }

    /* Do not trust st_size/fstat here: PS4's observed stat values do not match
       the byte stream. Verify the temporary file by reading and comparing it. */
    int verify_src = open(SOURCE_PRX, O_RDONLY);
    int verify_dst = open(DEST_TMP, O_RDONLY);
    if (verify_src < 0 || verify_dst < 0) {
        saved_errno = errno;
        if (verify_src >= 0) close(verify_src);
        if (verify_dst >= 0) close(verify_dst);
        remove(DEST_TMP);
        snprintf(error, error_size, "ERROR: cannot reopen PRX files for verification (errno=%d).", saved_errno);
        return 0;
    }

    char source_buffer[16384], destination_buffer[16384];
    long long verified_bytes = 0;
    for (;;) {
        ssize_t a = read_block(verify_src, source_buffer, sizeof(source_buffer));
        if (a < 0) { ok = 0; saved_errno = errno; break; }
        ssize_t b = read_block(verify_dst, destination_buffer, sizeof(destination_buffer));
        if (b < 0) { ok = 0; saved_errno = errno; break; }
        if (a != b || (a > 0 && memcmp(source_buffer, destination_buffer, (size_t)a) != 0)) {
            ok = 0;
            saved_errno = EIO;
            break;
        }
        if (a == 0) break;
        verified_bytes += a;
    }
    close(verify_src);
    close(verify_dst);

    if (!ok || verified_bytes != source_bytes) {
        remove(DEST_TMP);
        snprintf(error, error_size, "ERROR: PRX read-back verification failed (source=%lld verified=%lld errno=%d).",
                 source_bytes, verified_bytes, saved_errno);
        return 0;
    }
    /* Keep the verified PRX staged; main() commits it with plugins.ini. */
    return 1;
}
static void trim_line(char* line) {
    size_t n = strlen(line);
    while (n && (line[n-1] == '\r' || line[n-1] == '\n' ||
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
        while (pos < length && input[pos] != '\n') pos++;
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
    if (found_entry) {
        FILE* staged = fopen_staged_file(CONFIG_TMP);
        if (!staged) {
            snprintf(error, error_size, "ERROR: cannot stage unchanged plugins.ini (errno=%d).", errno);
            return 0;
        }
        int staged_ok = fwrite(input, 1, length, staged) == length;
        if (fflush(staged) != 0 || fsync(fileno(staged)) != 0) staged_ok = 0;
        if (fclose(staged) != 0) staged_ok = 0;
        if (!staged_ok) {
            int saved_errno = errno;
            remove(CONFIG_TMP);
            snprintf(error, error_size, "ERROR: cannot verify staged plugins.ini (errno=%d).", saved_errno);
            return 0;
        }
        return 1;
    }

    in_default = 0;
    pos = 0;
    while (pos < length) {
        size_t start = pos;
        while (pos < length && input[pos] != '\n') pos++;
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
            if (used && output[used-1] != '\n') output[used++] = '\n';
            int n = snprintf(output + used, sizeof(output)-used, "%s\n", DEST_PRX);
            if (n < 0 || (size_t)n >= sizeof(output)-used) {
                snprintf(error, error_size, "ERROR: no room to add Neural Engine to plugins.ini.");
                return 0;
            }
            used += (size_t)n;
        }
    }

    if (!found_default) {
        if (used && output[used-1] != '\n') output[used++] = '\n';
        const char* suffix = "\n[default]\n" DEST_PRX "\n";
        size_t n = strlen(suffix);
        if (used + n >= sizeof(output)) {
            snprintf(error, error_size, "ERROR: no room to create [default] in plugins.ini.");
            return 0;
        }
        memcpy(output + used, suffix, n);
        used += n;
    }

    FILE* out = fopen_staged_file(CONFIG_TMP);
    if (!out) {
        snprintf(error, error_size, "ERROR: cannot create temporary plugins.ini (errno=%d).", errno);
        return 0;
    }
    int ok = fwrite(output, 1, used, out) == used;
    if (fflush(out) != 0 || fsync(fileno(out)) != 0) ok = 0;
    if (fclose(out) != 0) ok = 0;
    if (!ok) {
        int saved_errno = errno;
        remove(CONFIG_TMP);
        snprintf(error, error_size, "ERROR: cannot stage updated plugins.ini (errno=%d).", saved_errno);
        return 0;
    }
    return 1;
}

/* Commit PRX and loader config with retained recovery copies. */
static int path_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int commit_install(char* error, size_t error_size) {
    int had_prx = path_exists(DEST_PRX);
    int had_config = path_exists(CONFIG);
    int moved_prx = 0, moved_config = 0, installed_prx = 0;
    int failure = 0;

    if (path_exists(DEST_BACKUP) || path_exists(CONFIG_BACKUP)) {
        snprintf(error, error_size, "ERROR: existing recovery backup found; refusing to overwrite it.");
        return 0;
    }
    if (had_prx) {
        if (rename(DEST_PRX, DEST_BACKUP) != 0) {
            snprintf(error, error_size, "ERROR: cannot back up existing PRX (errno=%d).", errno);
            return 0;
        }
        moved_prx = 1;
    }
    if (had_config) {
        if (rename(CONFIG, CONFIG_BACKUP) != 0) {
            int operation_errno = errno;
            int restore_errno = 0;
            if (moved_prx && rename(DEST_BACKUP, DEST_PRX) != 0) restore_errno = errno;
            if (restore_errno) {
                snprintf(error, error_size,
                         "ERROR: cannot back up plugins.ini (errno=%d); PRX restore also failed (errno=%d); inspect .pne-backup.",
                         operation_errno, restore_errno);
            } else {
                snprintf(error, error_size,
                         "ERROR: cannot back up plugins.ini (errno=%d); previous PRX restored.",
                         operation_errno);
            }
            return 0;
        }
        moved_config = 1;
    }
    if (rename(DEST_TMP, DEST_PRX) != 0) {
        failure = errno;
        goto rollback;
    }
    installed_prx = 1;
    if (rename(CONFIG_TMP, CONFIG) != 0) {
        failure = errno;
        goto rollback;
    }
    return 1;

rollback:
    {
        int rollback_errno = 0;
        if (installed_prx && remove(DEST_PRX) != 0 && errno != ENOENT) rollback_errno = errno;
        if (moved_config && rename(CONFIG_BACKUP, CONFIG) != 0 && rollback_errno == 0) rollback_errno = errno;
        if (moved_prx && rename(DEST_BACKUP, DEST_PRX) != 0 && rollback_errno == 0) rollback_errno = errno;
        remove(DEST_TMP);
        remove(CONFIG_TMP);
        if (rollback_errno) {
            snprintf(error, error_size,
                     "ERROR: transactional commit failed (errno=%d); rollback incomplete (errno=%d); inspect .pne-backup.",
                     failure, rollback_errno);
        } else {
            snprintf(error, error_size,
                     "ERROR: transactional commit failed (errno=%d); previous files restored.",
                     failure);
        }
    }
    return 0;
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
        remove(DEST_TMP);
        write_status(error);
        notify("PNE: falha ao preparar plugins.ini. Arquivos ativos preservados.");
        sceKernelUsleep(3000000);
        return 3;
    }
    if (!commit_install(error, sizeof(error))) {
        write_status(error);
        notify("PNE: falha na transacao. Verifique backups e install.log.");
        sceKernelUsleep(3000000);
        return 4;
    }

    write_status("OK: PRX and plugins.ini committed; previous files retained as .pne-backup when present. Restart GoldHEN or reboot to load.");
    notify("PNE instalado automaticamente. Reinicie o GoldHEN/PS4 para carregar.");
    sceKernelUsleep(3000000);
    return 0;
}
