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
#include "release_manifest.h"
#include "hybrid_state.h"
#include "sha256.h"

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
#define RELEASE_MANIFEST "/app0/pne-release.ini"
#define DEST_DIR "/data/GoldHEN/plugins"
#define DEST_PRX DEST_DIR "/ps4_neural_engine.prx"
#define DEST_TMP DEST_PRX ".pne-tmp"
#define DEST_BACKUP DEST_PRX ".pne-backup"
#define CONFIG "/data/GoldHEN/plugins.ini"
#define CONFIG_TMP CONFIG ".pne-tmp"
#define CONFIG_BACKUP CONFIG ".pne-backup"
#define INSTALLED_MANIFEST DEST_PRX ".pne-release.ini"
#define MANIFEST_TMP INSTALLED_MANIFEST ".pne-tmp"
#define MANIFEST_BACKUP INSTALLED_MANIFEST ".pne-backup"
#define TRANSACTION_MARKER DEST_PRX ".pne-transaction"
#define STATUS "/data/GoldHEN/ps4_neural_engine_install.log"
#define MAX_CONFIG 65536

/* Reserve a fresh staging path atomically. Existing stale files are never touched. */
static int open_unique_staged_file(const char* base, char* chosen, size_t capacity, int* fd_out) {
    for (unsigned index = 0; index < 10000; ++index) {
        int n = index == 0
            ? snprintf(chosen, capacity, "%s", base)
            : snprintf(chosen, capacity, "%s.%03u", base, index);
        if (n <= 0 || (size_t)n >= capacity) { errno = ENAMETOOLONG; return 0; }
        int fd = open(chosen, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) { *fd_out = fd; return 1; }
        if (errno != EEXIST) return 0;
    }
    errno = ENOSPC;
    return 0;
}

static FILE* fopen_unique_staged_file(const char* base, char* chosen, size_t capacity) {
    int fd = -1;
    if (!open_unique_staged_file(base, chosen, capacity, &fd)) return NULL;
    FILE* stream = fdopen(fd, "wb");
    if (!stream) {
        int saved_errno = errno;
        close(fd);
        remove(chosen);
        errno = saved_errno;
    }
    return stream;
}

static int path_exists(const char* path) {
    struct stat st;
    return lstat(path, &st) == 0;
}
static int read_release_number(const char* path, unsigned long* release) {
    FILE* file = fopen(path, "rb");
    if (!file) return 0;
    char line[256]; int found = 0;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "release_number=", 15) == 0) {
            char* end = NULL; errno = 0;
            unsigned long value = strtoul(line + 15, &end, 10);
            if (errno || end == line + 15 || (*end != '\n' && *end != '\r' && *end != '\\0')) { fclose(file); return 0; }
            *release = value; found = 1; break;
        }
    }
    if (ferror(file)) found = 0;
    fclose(file); return found;
}
static int stage_manifest(char* temp_path, size_t temp_path_size) {
    int in = open(RELEASE_MANIFEST, O_RDONLY);
    if (in < 0) return 0;
    int out = -1;
    if (!open_unique_staged_file(MANIFEST_TMP, temp_path, temp_path_size, &out)) { close(in); return 0; }
    char buffer[2048]; int ok = 1;
    for (;;) {
        ssize_t n = read_retry(in, buffer, sizeof(buffer));
        if (n == 0) break;
        if (n < 0) { ok = 0; break; }
        ssize_t offset = 0;
        while (offset < n) {
            ssize_t w = write(out, buffer + offset, (size_t)(n-offset));
            if (w < 0 && errno == EINTR) continue;
            if (w <= 0) { ok = 0; break; }
            offset += w;
        }
        if (!ok) break;
    }
    if (ok && fsync(out) != 0) ok = 0;
    if (close(in) != 0) ok = 0;
    if (close(out) != 0) ok = 0;
    if (!ok) remove(temp_path);
    return ok;
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

static int copy_prx(char* error, size_t error_size, char* temp_path, size_t temp_path_size) {
    int in = open(SOURCE_PRX, O_RDONLY);
    if (in < 0) {
        snprintf(error, error_size, "ERROR: cannot open source PRX %s (errno=%d).", SOURCE_PRX, errno);
        return 0;
    }

    int out = -1;
    if (!open_unique_staged_file(DEST_TMP, temp_path, temp_path_size, &out)) out = -1;
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
        remove(temp_path);
        snprintf(error, error_size, "ERROR: PRX copy failed after reading %lld and writing %lld bytes (errno=%d).",
                 source_bytes, written_bytes, saved_errno);
        return 0;
    }
    if (source_bytes <= 0 || written_bytes != source_bytes) {
        remove(temp_path);
        snprintf(error, error_size, "ERROR: PRX copy count mismatch (read=%lld written=%lld).",
                 source_bytes, written_bytes);
        return 0;
    }

    /* Do not trust st_size/fstat here: PS4's observed stat values do not match
       the byte stream. Verify the temporary file by reading and comparing it. */
    int verify_src = open(SOURCE_PRX, O_RDONLY);
    int verify_dst = open(temp_path, O_RDONLY);
    if (verify_src < 0 || verify_dst < 0) {
        saved_errno = errno;
        if (verify_src >= 0) close(verify_src);
        if (verify_dst >= 0) close(verify_dst);
        remove(temp_path);
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
        remove(temp_path);
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

static int configure_loader(char* error, size_t error_size, char* temp_path, size_t temp_path_size, int* changed) {
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
        *changed = 0;
        return 1;
    }
        if (!staged) {
            snprintf(error, error_size, "ERROR: cannot stage unchanged plugins.ini (errno=%d).", errno);
            return 0;
        }
        int staged_ok = fwrite(input, 1, length, staged) == length;
        if (fflush(staged) != 0 || fsync(fileno(staged)) != 0) staged_ok = 0;
        if (fclose(staged) != 0) staged_ok = 0;
        if (!staged_ok) {
            int saved_errno = errno;
            remove(temp_path);
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

    *changed = 1;\n    FILE* out = fopen_unique_staged_file(CONFIG_TMP, temp_path, temp_path_size);
    if (!out) {
        snprintf(error, error_size, "ERROR: cannot create temporary plugins.ini (errno=%d).", errno);
        return 0;
    }
    int ok = fwrite(output, 1, used, out) == used;
    if (fflush(out) != 0 || fsync(fileno(out)) != 0) ok = 0;
    if (fclose(out) != 0) ok = 0;
    if (!ok) {
        int saved_errno = errno;
        remove(temp_path);
        snprintf(error, error_size, "ERROR: cannot stage updated plugins.ini (errno=%d).", saved_errno);
        return 0;
    }
    return 1;
}

/* Commit PRX and loader config with retained, collision-free recovery copies. */
static int path_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* Never delete or overwrite a recovery copy. Rotate the new backup name instead. */
static int choose_backup_path(const char* base, char* output, size_t capacity) {
    if (!base || !output || capacity == 0) return 0;
    if (!path_exists(base)) {
        int n = snprintf(output, capacity, "%s", base);
        return n > 0 && (size_t)n < capacity;
    }
    for (unsigned index = 1; index < 10000; ++index) {
        int n = snprintf(output, capacity, "%s.%03u", base, index);
        if (n <= 0 || (size_t)n >= capacity) return 0;
        if (!path_exists(output)) return 1;
    }
    errno = ENOSPC;
    return 0;
}

static int commit_install(char* error, size_t error_size,
                          const char* prx_tmp, const char* manifest_tmp,
                          const char* config_tmp) {
    char prx_backup[256]={0}, manifest_backup[256]={0}, config_backup[256]={0};
    int had_prx=path_exists(DEST_PRX), had_manifest=path_exists(INSTALLED_MANIFEST);
    int had_config=config_tmp && config_tmp[0] && path_exists(CONFIG);
    int moved_prx=0,moved_manifest=0,moved_config=0,installed_prx=0,installed_manifest=0,installed_config=0;
    int failure=0, marker_fd=-1;
    if (had_prx && !choose_backup_path(DEST_BACKUP,prx_backup,sizeof(prx_backup))) goto reserve_failed;
    if (had_manifest && !choose_backup_path(MANIFEST_BACKUP,manifest_backup,sizeof(manifest_backup))) goto reserve_failed;
    if (had_config && !choose_backup_path(CONFIG_BACKUP,config_backup,sizeof(config_backup))) goto reserve_failed;
    marker_fd=open(TRANSACTION_MARKER,O_WRONLY|O_CREAT|O_EXCL,0600);
    if(marker_fd<0){snprintf(error,error_size,"ERROR: RECOVERY_REQUIRED; transaction marker cannot be created (errno=%d).",errno);return 0;}
    {
        const char marker[]="PNE transaction in progress; preserve all backups.\n";
        if(write(marker_fd,marker,sizeof(marker)-1)!=(ssize_t)(sizeof(marker)-1)||fsync(marker_fd)!=0){
            failure=errno?errno:EIO;close(marker_fd);remove(TRANSACTION_MARKER);
            snprintf(error,error_size,"ERROR: cannot persist transaction marker (errno=%d).",failure);return 0;
        }
    }
    close(marker_fd);
    if(had_prx){if(rename(DEST_PRX,prx_backup)!=0){failure=errno;goto rollback;}moved_prx=1;}
    if(had_manifest){if(rename(INSTALLED_MANIFEST,manifest_backup)!=0){failure=errno;goto rollback;}moved_manifest=1;}
    if(had_config){if(rename(CONFIG,config_backup)!=0){failure=errno;goto rollback;}moved_config=1;}
    if(rename(prx_tmp,DEST_PRX)!=0){failure=errno;goto rollback;}installed_prx=1;
    if(rename(manifest_tmp,INSTALLED_MANIFEST)!=0){failure=errno;goto rollback;}installed_manifest=1;
    if(config_tmp&&config_tmp[0]){if(rename(config_tmp,CONFIG)!=0){failure=errno;goto rollback;}installed_config=1;}
    if(remove(TRANSACTION_MARKER)!=0){snprintf(error,error_size,"ERROR: files committed but marker remains; RECOVERY_REQUIRED (errno=%d).",errno);return 0;}
    return 1;
reserve_failed:
    snprintf(error,error_size,"ERROR: cannot reserve recovery backup path (errno=%d).",errno);return 0;
rollback:
    {
        int re=0;
        if(installed_config&&remove(CONFIG)!=0&&errno!=ENOENT)re=errno;
        if(installed_manifest&&remove(INSTALLED_MANIFEST)!=0&&errno!=ENOENT&&!re)re=errno;
        if(installed_prx&&remove(DEST_PRX)!=0&&errno!=ENOENT&&!re)re=errno;
        if(moved_config&&rename(config_backup,CONFIG)!=0&&!re)re=errno;
        if(moved_manifest&&rename(manifest_backup,INSTALLED_MANIFEST)!=0&&!re)re=errno;
        if(moved_prx&&rename(prx_backup,DEST_PRX)!=0&&!re)re=errno;
        if(prx_tmp&&prx_tmp[0])remove(prx_tmp);
        if(manifest_tmp&&manifest_tmp[0])remove(manifest_tmp);
        if(config_tmp&&config_tmp[0])remove(config_tmp);
        if(re) snprintf(error,error_size,"ERROR: transaction failed (errno=%d); rollback incomplete (errno=%d); marker retained; inspect backups.",failure,re);
        else {remove(TRANSACTION_MARKER);snprintf(error,error_size,"ERROR: transaction failed (errno=%d); previous files restored.",failure);}
    }
    return 0;
}

int main(void) {
    char error[256],prx_tmp[256]={0},manifest_tmp[256]={0},config_tmp[256]={0};
    int config_changed=0;
    unsigned long incoming_release=0,installed_release=0;
    char incoming_hash[65],installed_hash[65];
    if(!pne_validate_release_manifest(RELEASE_MANIFEST,SOURCE_PRX,error,sizeof(error))||
       !read_release_number(RELEASE_MANIFEST,&incoming_release)||
       !pne_sha256_file(SOURCE_PRX,incoming_hash)){
        write_status("ABORTED: invalid incoming manifest/hash; no destination changed.");
        notify("PNE: manifesto/hash do pacote invalido.");sceKernelUsleep(3000000);return 5;
    }
    int prx_exists=path_exists(DEST_PRX),manifest_exists=path_exists(INSTALLED_MANIFEST);
    int marker_exists=path_exists(TRANSACTION_MARKER);
    int manifest_valid=manifest_exists&&prx_exists&&
        pne_validate_release_manifest(INSTALLED_MANIFEST,DEST_PRX,error,sizeof(error));
    pne_install_state state=pne_classify_install_state(prx_exists,manifest_exists,manifest_valid,marker_exists);
    if(state==PNE_STATE_RECOVERY||state==PNE_STATE_CONFLICT){
        snprintf(error,sizeof(error),"ABORTED: state=%s; files/backups preserved; inspect transaction marker.",pne_install_state_name(state));
        write_status(error);notify("PNE: recuperacao/conflito detectado; nada alterado.");sceKernelUsleep(3000000);return 6;
    }
    if(state==PNE_STATE_UPDATE){
        if(!read_release_number(INSTALLED_MANIFEST,&installed_release)||!pne_sha256_file(DEST_PRX,installed_hash)){
            write_status("ABORTED: installed manifest unreadable; CONFLICT.");return 6;
        }
        int order=pne_compare_release(installed_release,incoming_release);
        if(order<0){write_status("ABORTED: downgrade refused; installed release is newer.");notify("PNE: downgrade bloqueado.");sceKernelUsleep(3000000);return 7;}
        if(order==0){
            if(strcmp(installed_hash,incoming_hash)==0){
                write_status("ALREADY_CURRENT: release and PRX SHA-256 match; no writes or backups created.");
                notify("PNE: Neural Engine ja esta atualizado.");sceKernelUsleep(2500000);return 0;
            }
            write_status("ABORTED: CONFLICT; same release number with different PRX hash.");notify("PNE: conflito de hash; nada alterado.");sceKernelUsleep(3000000);return 8;
        }
    }
    if(mkdir("/data/GoldHEN",0777)!=0&&errno!=EEXIST){snprintf(error,sizeof(error),"ERROR: cannot create GoldHEN directory (errno=%d).",errno);write_status(error);return 1;}
    if(mkdir(DEST_DIR,0777)!=0&&errno!=EEXIST){snprintf(error,sizeof(error),"ERROR: cannot create plugin directory (errno=%d).",errno);write_status(error);return 1;}
    if(!copy_prx(error,sizeof(error),prx_tmp,sizeof(prx_tmp))){write_status(error);notify("PNE: falha ao copiar PRX.");sceKernelUsleep(3000000);return 2;}
    if(!stage_manifest(manifest_tmp,sizeof(manifest_tmp))){int e=errno;remove(prx_tmp);snprintf(error,sizeof(error),"ERROR: cannot stage installed manifest (errno=%d).",e);write_status(error);return 3;}
    if(!configure_loader(error,sizeof(error),config_tmp,sizeof(config_tmp),&config_changed)){
        remove(prx_tmp);remove(manifest_tmp);write_status(error);notify("PNE: falha ao preparar plugins.ini.");sceKernelUsleep(3000000);return 3;
    }
    if(!config_changed)config_tmp[0]='\\0';
    if(!commit_install(error,sizeof(error),prx_tmp,manifest_tmp,config_tmp)){
        write_status(error);notify("PNE: transacao nao concluida; preserve backups.");sceKernelUsleep(3000000);return 4;
    }
    if(state==PNE_STATE_NEW)write_status("NEW_INSTALL: PRX, manifest and loader registration committed.");
    else if(state==PNE_STATE_LEGACY)write_status("LEGACY_MIGRATION: unverified old PRX preserved as backup; managed manifest installed.");
    else write_status("UPDATE: newer release committed; downgrade protection active; backups retained.");
    notify("PNE instalado/atualizado. Reinicie GoldHEN/PS4 para carregar.");sceKernelUsleep(3000000);return 0;
}
