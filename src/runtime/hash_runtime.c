/**
 * @file hash_runtime.c
 * @brief SHA-256 file hashing for reproducible package and build contracts.
 */

#include "process_runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#endif

typedef struct {
    uint32_t state[8];
    uint64_t total_bytes;
    unsigned char block[64];
    size_t block_len;
} BaaSha256;

static const uint32_t sha256_k[64] = {
    UINT32_C(0x428a2f98), UINT32_C(0x71374491), UINT32_C(0xb5c0fbcf), UINT32_C(0xe9b5dba5),
    UINT32_C(0x3956c25b), UINT32_C(0x59f111f1), UINT32_C(0x923f82a4), UINT32_C(0xab1c5ed5),
    UINT32_C(0xd807aa98), UINT32_C(0x12835b01), UINT32_C(0x243185be), UINT32_C(0x550c7dc3),
    UINT32_C(0x72be5d74), UINT32_C(0x80deb1fe), UINT32_C(0x9bdc06a7), UINT32_C(0xc19bf174),
    UINT32_C(0xe49b69c1), UINT32_C(0xefbe4786), UINT32_C(0x0fc19dc6), UINT32_C(0x240ca1cc),
    UINT32_C(0x2de92c6f), UINT32_C(0x4a7484aa), UINT32_C(0x5cb0a9dc), UINT32_C(0x76f988da),
    UINT32_C(0x983e5152), UINT32_C(0xa831c66d), UINT32_C(0xb00327c8), UINT32_C(0xbf597fc7),
    UINT32_C(0xc6e00bf3), UINT32_C(0xd5a79147), UINT32_C(0x06ca6351), UINT32_C(0x14292967),
    UINT32_C(0x27b70a85), UINT32_C(0x2e1b2138), UINT32_C(0x4d2c6dfc), UINT32_C(0x53380d13),
    UINT32_C(0x650a7354), UINT32_C(0x766a0abb), UINT32_C(0x81c2c92e), UINT32_C(0x92722c85),
    UINT32_C(0xa2bfe8a1), UINT32_C(0xa81a664b), UINT32_C(0xc24b8b70), UINT32_C(0xc76c51a3),
    UINT32_C(0xd192e819), UINT32_C(0xd6990624), UINT32_C(0xf40e3585), UINT32_C(0x106aa070),
    UINT32_C(0x19a4c116), UINT32_C(0x1e376c08), UINT32_C(0x2748774c), UINT32_C(0x34b0bcb5),
    UINT32_C(0x391c0cb3), UINT32_C(0x4ed8aa4a), UINT32_C(0x5b9cca4f), UINT32_C(0x682e6ff3),
    UINT32_C(0x748f82ee), UINT32_C(0x78a5636f), UINT32_C(0x84c87814), UINT32_C(0x8cc70208),
    UINT32_C(0x90befffa), UINT32_C(0xa4506ceb), UINT32_C(0xbef9a3f7), UINT32_C(0xc67178f2),
};

static uint32_t rotr32(uint32_t value, unsigned count)
{
    return (value >> count) | (value << (32u - count));
}

static uint32_t load_be32(const unsigned char* data)
{
    return ((uint32_t)data[0] << 24u) | ((uint32_t)data[1] << 16u) |
           ((uint32_t)data[2] << 8u) | (uint32_t)data[3];
}

static void sha256_transform(BaaSha256* ctx, const unsigned char block[64])
{
    uint32_t w[64];
    for (size_t i = 0; i < 16u; ++i) w[i] = load_be32(block + i * 4u);
    for (size_t i = 16u; i < 64u; ++i) {
        uint32_t s0 = rotr32(w[i - 15u], 7u) ^ rotr32(w[i - 15u], 18u) ^ (w[i - 15u] >> 3u);
        uint32_t s1 = rotr32(w[i - 2u], 17u) ^ rotr32(w[i - 2u], 19u) ^ (w[i - 2u] >> 10u);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }

    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];
    for (size_t i = 0; i < 64u; ++i) {
        uint32_t sum1 = rotr32(e, 6u) ^ rotr32(e, 11u) ^ rotr32(e, 25u);
        uint32_t choose = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + sum1 + choose + sha256_k[i] + w[i];
        uint32_t sum0 = rotr32(a, 2u) ^ rotr32(a, 13u) ^ rotr32(a, 22u);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(BaaSha256* ctx)
{
    static const uint32_t initial[8] = {
        UINT32_C(0x6a09e667), UINT32_C(0xbb67ae85), UINT32_C(0x3c6ef372), UINT32_C(0xa54ff53a),
        UINT32_C(0x510e527f), UINT32_C(0x9b05688c), UINT32_C(0x1f83d9ab), UINT32_C(0x5be0cd19),
    };
    for (size_t i = 0; i < 8u; ++i) ctx->state[i] = initial[i];
    ctx->total_bytes = 0;
    ctx->block_len = 0;
}

static void sha256_update(BaaSha256* ctx, const unsigned char* data, size_t len)
{
    ctx->total_bytes += (uint64_t)len;
    while (len > 0u) {
        size_t available = 64u - ctx->block_len;
        size_t take = len < available ? len : available;
        for (size_t i = 0; i < take; ++i) ctx->block[ctx->block_len + i] = data[i];
        ctx->block_len += take;
        data += take;
        len -= take;
        if (ctx->block_len == 64u) {
            sha256_transform(ctx, ctx->block);
            ctx->block_len = 0;
        }
    }
}

static void sha256_final(BaaSha256* ctx, unsigned char digest[32])
{
    size_t pos = ctx->block_len;
    ctx->block[pos++] = 0x80u;
    if (pos > 56u) {
        while (pos < 64u) ctx->block[pos++] = 0;
        sha256_transform(ctx, ctx->block);
        pos = 0;
    }
    while (pos < 56u) ctx->block[pos++] = 0;
    uint64_t bits = ctx->total_bytes * UINT64_C(8);
    for (size_t i = 0; i < 8u; ++i) {
        ctx->block[63u - i] = (unsigned char)(bits & UINT64_C(0xff));
        bits >>= 8u;
    }
    sha256_transform(ctx, ctx->block);
    for (size_t i = 0; i < 8u; ++i) {
        digest[i * 4u] = (unsigned char)(ctx->state[i] >> 24u);
        digest[i * 4u + 1u] = (unsigned char)(ctx->state[i] >> 16u);
        digest[i * 4u + 2u] = (unsigned char)(ctx->state[i] >> 8u);
        digest[i * 4u + 3u] = (unsigned char)ctx->state[i];
    }
}

static char* hash_text_to_utf8(const BaaRuntimeChar* text)
{
    if (!text) return NULL;
    size_t bytes = 0;
    for (size_t i = 0; text[i] != 0; ++i) {
        unsigned len = (unsigned)((text[i] >> 32) & UINT64_C(0xff));
        if (len < 1u || len > 4u || bytes > SIZE_MAX - len) return NULL;
        bytes += len;
    }
    char* out = (char*)malloc(bytes + 1u);
    if (!out) return NULL;
    size_t pos = 0;
    for (size_t i = 0; text[i] != 0; ++i) {
        unsigned len = (unsigned)((text[i] >> 32) & UINT64_C(0xff));
        uint32_t raw = (uint32_t)(text[i] & UINT64_C(0xffffffff));
        for (unsigned j = 0; j < len; ++j) out[pos++] = (char)((raw >> (j * 8u)) & 0xffu);
    }
    out[pos] = '\0';
    return out;
}

static FILE* hash_open_file(const char* path)
{
#ifdef _WIN32
    int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (required <= 0) return NULL;
    wchar_t* wide = (wchar_t*)malloc((size_t)required * sizeof(wchar_t));
    if (!wide) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, required)) {
        free(wide);
        return NULL;
    }
    FILE* file = _wfopen(wide, L"rb");
    free(wide);
    return file;
#else
    return fopen(path, "rb");
#endif
}

BaaRuntimeChar* baa_runtime_sha256_file(const BaaRuntimeChar* baa_path)
{
    char* path = hash_text_to_utf8(baa_path);
    if (!path) return NULL;
    FILE* file = hash_open_file(path);
    free(path);
    if (!file) return NULL;

    BaaSha256 ctx;
    sha256_init(&ctx);
    unsigned char buffer[16384];
    size_t got = 0;
    while ((got = fread(buffer, 1u, sizeof(buffer), file)) > 0u) sha256_update(&ctx, buffer, got);
    if (ferror(file)) {
        fclose(file);
        return NULL;
    }
    if (fclose(file) != 0) return NULL;

    unsigned char digest[32];
    sha256_final(&ctx, digest);
    static const char hex[] = "0123456789abcdef";
    BaaRuntimeChar* out = (BaaRuntimeChar*)calloc(65u, sizeof(BaaRuntimeChar));
    if (!out) return NULL;
    for (size_t i = 0; i < 32u; ++i) {
        out[i * 2u] = (UINT64_C(1) << 32) | (unsigned char)hex[digest[i] >> 4u];
        out[i * 2u + 1u] = (UINT64_C(1) << 32) | (unsigned char)hex[digest[i] & 0x0fu];
    }
    return out;
}

