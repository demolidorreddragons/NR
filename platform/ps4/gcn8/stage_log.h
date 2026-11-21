#ifndef STAGE_LOG_H
#define STAGE_LOG_H
#ifdef __cplusplus
extern "C" {
#endif
enum { STAGE_LOG_FILE_OK = 1u << 0, STAGE_LOG_UDP_OK = 1u << 1 };
/* Returns active transport bitmask. UDP is optional and requires STAGE_LOG_UDP. */
int stage_log_init(const char *path, const char *udp_ip, int udp_port);
/* Returns 0 when all fatal handlers are installed; falls back without SA_ONSTACK if altstack fails. */
int stage_log_install_signals(void);
/* Bounded stage record; normal-context only. File is synced before return. */
void stage_log(const char *stage);
#ifdef __cplusplus
}
#endif
#endif
