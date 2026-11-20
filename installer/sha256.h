#ifndef PNE_SHA256_H
#define PNE_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t block[64];
    size_t block_length;
} PNE_SHA256_CTX;

void pne_sha256_init(PNE_SHA256_CTX* context);
void pne_sha256_update(PNE_SHA256_CTX* context, const void* data, size_t length);
void pne_sha256_final(PNE_SHA256_CTX* context, uint8_t digest[32]);
void pne_sha256_hex(const uint8_t digest[32], char output[65]);
int pne_sha256_file(const char* path, char output[65]);

#endif
