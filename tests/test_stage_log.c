#define _POSIX_C_SOURCE 200809L
#include "stage_log.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void read_file_contains(const char *path, const char *needle) {
    FILE *f = fopen(path, "rb");
    assert(f != NULL);
    char data[1024];
    size_t n = fread(data, 1, sizeof(data) - 1, f);
    fclose(f);
    data[n] = '\0';
    assert(strstr(data, needle) != NULL);
}

int main(void) {
    char path[] = "/tmp/pne-stage-log-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    int status = stage_log_init(path, NULL, 0);
    assert((status & STAGE_LOG_FILE_OK) != 0);
    stage_log("MAIN_ENTERED");
    char long_stage[512];
    memset(long_stage, 'A', sizeof(long_stage) - 1);
    long_stage[sizeof(long_stage) - 1] = '\0';
    stage_log(long_stage);
    read_file_contains(path, "LOG_OPEN anchor=0x");
    read_file_contains(path, "STAGE=MAIN_ENTERED\n");
    read_file_contains(path, "STAGE=AAAAAAAA");

    char crash_path[] = "/tmp/pne-stage-crash-XXXXXX";
    fd = mkstemp(crash_path);
    assert(fd >= 0);
    close(fd);
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert((stage_log_init(crash_path, NULL, 0) & STAGE_LOG_FILE_OK) != 0);
        assert(stage_log_install_signals() == 0);
        raise(SIGABRT);
        _exit(1);
    }
    int wait_status = 0;
    assert(waitpid(child, &wait_status, 0) == child);
    assert(WIFEXITED(wait_status));
    assert(WEXITSTATUS(wait_status) == 128 + SIGABRT);
    read_file_contains(crash_path, "CRASH sig=0x");
    unlink(crash_path);
    unlink(path);
    puts("stage_log host tests: PASS");
    return 0;
}
