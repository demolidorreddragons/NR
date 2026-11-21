#define _POSIX_C_SOURCE 200809L
#include "stage_log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void) {
    char path[]="/tmp/pne-stage-log-XXXXXX";
    int fd=mkstemp(path); assert(fd>=0); close(fd);
    int status=stage_log_init(path,NULL,0);
    assert((status & STAGE_LOG_FILE_OK)!=0);
    stage_log("MAIN_ENTERED");
    char long_stage[512]; memset(long_stage,'A',sizeof(long_stage)-1);
    long_stage[sizeof(long_stage)-1]='\\0'; stage_log(long_stage);
    FILE *f=fopen(path,"rb"); assert(f!=NULL);
    char data[512]; size_t n=fread(data,1,sizeof(data)-1,f); fclose(f); data[n]='\\0';
    assert(strstr(data,"LOG_OPEN anchor=0x")!=NULL);
    assert(strstr(data,"STAGE=MAIN_ENTERED\\n")!=NULL);
    assert(strstr(data,"STAGE=AAAAAAAA")!=NULL);
    unlink(path); puts("stage_log host tests: PASS"); return 0;
}
