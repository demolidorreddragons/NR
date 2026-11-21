#define _POSIX_C_SOURCE 200809L
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include "stage_log.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef STAGE_LOG_UDP
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#if !defined(__x86_64__)
#error "stage_log crash context currently supports x86_64 only"
#endif
static int g_fd = -1;
static int g_initialized = 0;
static char g_altstack[64 * 1024] __attribute__((aligned(16)));
#ifdef STAGE_LOG_UDP
static int g_sock = -1;
static struct sockaddr_in g_dst;
static void write_udp_record(const char *buf, size_t len) {
    /* write() on a connected datagram socket is async-signal-safe. */
    if (g_sock >= 0) (void)write(g_sock, buf, len);
}
#endif
static size_t append_text(char *out, size_t cap, size_t n, const char *s) {
    if (!s) s = "(null)";
    while (*s && n < cap) out[n++] = *s++;
    return n;
}
static size_t append_hex(char *out, size_t cap, size_t n, uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    if (n + 18 > cap) return n;
    out[n++]='0'; out[n++]='x';
    for (int shift=60; shift>=0; shift-=4) out[n++]=digits[(value>>shift)&0xfu];
    return n;
}
/* write() is async-signal-safe. No fsync/sendto/stdio/allocation in crash path. */
static void write_record(int fd, const char *buf, size_t len) {
    if (fd < 0) return;
    while (len) {
        ssize_t written = write(fd, buf, len);
        if (written > 0) { buf += (size_t)written; len -= (size_t)written; }
        else if (written < 0 && errno == EINTR) continue;
        else break;
    }
}
static void emit_normal(const char *buf, size_t len) {
    write_record(g_fd, buf, len);
    if (g_fd >= 0) (void)fsync(g_fd);
#ifdef STAGE_LOG_UDP
    write_udp_record(buf, len);
#endif
}
int stage_log_init(const char *path, const char *udp_ip, int udp_port) {
    int status=0;
    if (g_fd >= 0) { close(g_fd); g_fd=-1; }
#ifdef STAGE_LOG_UDP
    if (g_sock >= 0) { close(g_sock); g_sock=-1; }
#endif
    if (path && path[0]) g_fd=open(path,O_WRONLY|O_CREAT|O_APPEND,0666);
    if (g_fd >= 0) status |= STAGE_LOG_FILE_OK;
#ifdef STAGE_LOG_UDP
    if (udp_ip && udp_ip[0] && udp_port>0 && udp_port<=65535) {
        struct in_addr parsed;
        if (inet_pton(AF_INET,udp_ip,&parsed)==1) {
            g_sock=socket(AF_INET,SOCK_DGRAM,0);
            if (g_sock>=0) {
                memset(&g_dst,0,sizeof(g_dst)); g_dst.sin_family=AF_INET;
                g_dst.sin_port=htons((uint16_t)udp_port); g_dst.sin_addr=parsed;
                if (connect(g_sock,(const struct sockaddr *)&g_dst,sizeof(g_dst))==0 &&
                    fcntl(g_sock,F_SETFL,O_NONBLOCK)==0) {
                    status |= STAGE_LOG_UDP_OK;
                } else {
                    close(g_sock);
                    g_sock=-1;
                }
            }
        }
    }
#else
    (void)udp_ip; (void)udp_port;
#endif
    g_initialized=1;
    char line[96]; size_t n=0;
    n=append_text(line,sizeof(line),n,"LOG_OPEN anchor=");
    n=append_hex(line,sizeof(line),n,(uint64_t)(uintptr_t)&stage_log_init);
    if (n<sizeof(line)) line[n++]='\n';
    emit_normal(line,n);
    return status;
}
void stage_log(const char *stage) {
    char line[160]; size_t n=0;
    n=append_text(line,sizeof(line)-1,n,"STAGE=");
    if (!stage) stage="(null)";
    while (*stage && n<sizeof(line)-1) {
        unsigned char c=(unsigned char)*stage++;
        line[n++]=(c>=0x20 && c!=0x7f)?(char)c:'_';
    }
    line[n++]='\n';
    if (g_initialized) emit_normal(line,n);
}
static size_t append_dec(char *out, size_t cap, size_t n, unsigned value) {
    char digits[10]; size_t count=0;
    do { digits[count++]=(char)('0'+(value%10u)); value/=10u; } while(value && count<sizeof(digits));
    while(count && n<cap) out[n++]=digits[--count];
    return n;
}
static void emit_crash_line(const char *line, size_t len) {
    write_record(g_fd,line,len);
#ifdef STAGE_LOG_UDP
    write_udp_record(line,len);
#endif
}
static void on_signal(int sig, siginfo_t *info, void *context) {
    /* OpenOrbis v0.5.4 exposes musl ucontext declarations, while the PS4
     * kernel supplies a FreeBSD-derived signal frame. Do not cast it to the
     * SDK's incompatible ucontext_t: preserve raw words for offline decoding. */
    const volatile uint64_t *words=(const volatile uint64_t *)context;
    char line[256]; size_t n=0;
    n=append_text(line,sizeof(line),n,"CRASH sig=");
    n=append_hex(line,sizeof(line),n,(uint64_t)(unsigned)sig);
    n=append_text(line,sizeof(line),n," addr=");
    n=append_hex(line,sizeof(line),n,info?(uint64_t)(uintptr_t)info->si_addr:0);
    n=append_text(line,sizeof(line),n," anchor=");
    n=append_hex(line,sizeof(line),n,(uint64_t)(uintptr_t)&stage_log_init);
    n=append_text(line,sizeof(line),n," ctx_w22=");
    n=append_hex(line,sizeof(line),n,words[22]);
    n=append_text(line,sizeof(line),n," ctx_w25=");
    n=append_hex(line,sizeof(line),n,words[25]);
    n=append_text(line,sizeof(line),n," ctx_w28=");
    n=append_hex(line,sizeof(line),n,words[28]);
    n=append_text(line,sizeof(line),n," ctx_w31=");
    n=append_hex(line,sizeof(line),n,words[31]);
    if (n<sizeof(line)) line[n++]='\n';
    emit_crash_line(line,n);

    /* The context ABI offsets differ across OpenOrbis headers and the PS4
     * kernel. Dump a bounded prefix so RIP/RSP can be decoded later without
     * inventing a register layout. */
    for (unsigned base=0;base<48;base+=4) {
        n=0;
        n=append_text(line,sizeof(line),n,"CRASH_CTX[");
        n=append_dec(line,sizeof(line),n,base);
        n=append_text(line,sizeof(line),n,"]");
        for (unsigned i=0;i<4;i++) {
            n=append_text(line,sizeof(line),n," ");
            n=append_hex(line,sizeof(line),n,words[base+i]);
        }
        if (n<sizeof(line)) line[n++]='\n';
        emit_crash_line(line,n);
    }
    _exit(128+(sig&0x7f));
}
static void log_signal_diag(const char *operation, unsigned signal_number,
                              int result, int saved_errno) {
    char line[160]; size_t n=0;
    n=append_text(line,sizeof(line),n,"SIGNAL_DIAG op=");
    n=append_text(line,sizeof(line),n,operation);
    n=append_text(line,sizeof(line),n," signal=");
    n=append_dec(line,sizeof(line),n,signal_number);
    n=append_text(line,sizeof(line),n," rc=");
    n=append_text(line,sizeof(line),n,result < 0 ? "-1" : "0");
    n=append_text(line,sizeof(line),n," errno=");
    n=append_dec(line,sizeof(line),n,(unsigned)saved_errno);
    stage_log(line);
}

static void log_signal_abi(void) {
    char line[192]; size_t n=0;
    n=append_text(line,sizeof(line),n,"SIGNAL_ABI stack_t_size=");
    n=append_dec(line,sizeof(line),n,(unsigned)sizeof(stack_t));
    n=append_text(line,sizeof(line),n," ss_sp_off=");
    n=append_dec(line,sizeof(line),n,(unsigned)offsetof(stack_t,ss_sp));
    n=append_text(line,sizeof(line),n," ss_size_off=");
    n=append_dec(line,sizeof(line),n,(unsigned)offsetof(stack_t,ss_size));
    n=append_text(line,sizeof(line),n," ss_flags_off=");
    n=append_dec(line,sizeof(line),n,(unsigned)offsetof(stack_t,ss_flags));
    n=append_text(line,sizeof(line),n," altstack_bytes=");
    n=append_dec(line,sizeof(line),n,(unsigned)sizeof(g_altstack));
    stage_log(line);
}

int stage_log_install_signals(void) {
    stage_log("SIGNAL_INSTALL_BEGIN");
    log_signal_abi();

    stack_t ss; memset(&ss,0,sizeof(ss));
    ss.ss_sp=g_altstack; ss.ss_size=sizeof(g_altstack);
    errno=0;
    int rc=sigaltstack(&ss,NULL);
    int saved_errno=(rc==0)?0:errno;
    const int altstack_ready=(rc==0);
    log_signal_diag("sigaltstack",0u,rc,saved_errno);
    if (!altstack_ready)
        stage_log("SIGNAL_ALTSTACK_UNAVAILABLE_FALLBACK_NO_SA_ONSTACK");

    struct sigaction action; memset(&action,0,sizeof(action));
    #if defined(__FreeBSD__) && !defined(__linux__)
    action.__sa_handler.__sa_sigaction=on_signal;
#else
    action.sa_sigaction=on_signal;
#endif
    action.sa_flags=SA_SIGINFO|(altstack_ready?SA_ONSTACK:0);
    errno=0;
    rc=sigemptyset(&action.sa_mask);
    saved_errno=(rc==0)?0:errno;
    log_signal_diag("sigemptyset",0u,rc,saved_errno);
    if (rc!=0) return -1;

    const int signals[]={SIGSEGV,SIGBUS,SIGILL,SIGABRT,SIGFPE};
    int action_failures=0;
    for (size_t i=0;i<sizeof(signals)/sizeof(signals[0]);++i) {
        errno=0;
        rc=sigaction(signals[i],&action,NULL);
        saved_errno=(rc==0)?0:errno;
        log_signal_diag("sigaction",(unsigned)signals[i],rc,saved_errno);
        if (rc!=0) ++action_failures;
    }
    if (action_failures!=0) {
        stage_log("SIGNAL_INSTALL_PARTIAL");
        return -1;
    }
    stage_log(altstack_ready ? "SIGNAL_INSTALL_COMPLETE" :
                               "SIGNAL_INSTALL_COMPLETE_NO_ALTSTACK");
    return 0;
}
