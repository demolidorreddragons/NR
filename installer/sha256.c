#include "sha256.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static const uint32_t k[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

static uint32_t rotate_right(uint32_t value, unsigned count) {
    return (value >> count) | (value << (32u - count));
}

static uint32_t load_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void store_be32(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static void transform(PNE_SHA256_CTX* context, const uint8_t block[64]) {
    uint32_t w[64];
    for (unsigned i = 0; i < 16; ++i) w[i] = load_be32(block + i * 4u);
    for (unsigned i = 16; i < 64; ++i) {
        uint32_t s0 = rotate_right(w[i - 15], 7) ^ rotate_right(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotate_right(w[i - 2], 17) ^ rotate_right(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = context->state[0], b = context->state[1];
    uint32_t c = context->state[2], d = context->state[3];
    uint32_t e = context->state[4], f = context->state[5];
    uint32_t g = context->state[6], h = context->state[7];

    for (unsigned i = 0; i < 64; ++i) {
        uint32_t s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        uint32_t choice = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + choice + k[i] + w[i];
        uint32_t s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + majority;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    context->state[0] += a; context->state[1] += b;
    context->state[2] += c; context->state[3] += d;
    context->state[4] += e; context->state[5] += f;
    context->state[6] += g; context->state[7] += h;
}

void pne_sha256_init(PNE_SHA256_CTX* context) {
    static const uint32_t initial[8] = {
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u
    };
    memcpy(context->state, initial, sizeof(initial));
    context->bit_count = 0;
    context->block_length = 0;
}

void pne_sha256_update(PNE_SHA256_CTX* context, const void* data, size_t length) {
    const uint8_t* input = (const uint8_t*)data;
    if (!length) return;
    context->bit_count += (uint64_t)length * 8u;
    while (length) {
        size_t available = sizeof(context->block) - context->block_length;
        size_t take = length < available ? length : available;
        memcpy(context->block + context->block_length, input, take);
        context->block_length += take;
        input += take;
        length -= take;
        if (context->block_length == sizeof(context->block)) {
            transform(context, context->block);
            context->block_length = 0;
        }
    }
}

void pne_sha256_final(PNE_SHA256_CTX* context, uint8_t digest[32]) {
    uint64_t bit_count = context->bit_count;
    context->block[context->block_length++] = 0x80;
    if (context->block_length > 56) {
        memset(context->block + context->block_length, 0, 64 - context->block_length);
        transform(context, context->block);
        context->block_length = 0;
    }
    memset(context->block + context->block_length, 0, 56 - context->block_length);
    for (unsigned i = 0; i < 8; ++i)
        context->block[63 - i] = (uint8_t)(bit_count >> (i * 8u));
    transform(context, context->block);
    for (unsigned i = 0; i < 8; ++i) store_be32(digest + i * 4u, context->state[i]);
    memset(context, 0, sizeof(*context));
}

void pne_sha256_hex(const uint8_t digest[32], char output[65]) {
    static const char hex[] = "0123456789abcdef";
    for (unsigned i = 0; i < 32; ++i) {
        output[i * 2] = hex[digest[i] >> 4];
        output[i * 2 + 1] = hex[digest[i] & 0x0f];
    }
    output[64] = '\0';
}

int pne_sha256_file(const char* path, char output[65]) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    PNE_SHA256_CTX context;
    pne_sha256_init(&context);
    uint8_t buffer[16384];
    int ok = 1;
    for (;;) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { ok = 0; break; }
        if (n == 0) break;
        pne_sha256_update(&context, buffer, (size_t)n);
    }
    int saved_errno = errno;
    if (close(fd) != 0 && ok) { ok = 0; saved_errno = errno; }
    if (!ok) { errno = saved_errno; return 0; }
    uint8_t digest[32];
    pne_sha256_final(&context, digest);
    pne_sha256_hex(digest, output);
    return 1;
}
