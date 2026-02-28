/* Kripto modülü — çalışma zamanı implementasyonu */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "runtime.h"

/* ========== MODUL 7: Kripto/Hash ========== */

/* --- MD5 (RFC 1321) --- */

static const uint32_t _md5_s[64] = {
    7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
    5, 9,14,20, 5, 9,14,20, 5, 9,14,20, 5, 9,14,20,
    4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
    6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21
};

static const uint32_t _md5_K[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,
    0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,
    0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,
    0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,
    0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,
    0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,
    0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,
    0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,
    0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};

static uint32_t _md5_left_rotate(uint32_t x, uint32_t c) {
    return (x << c) | (x >> (32 - c));
}

static void _md5_compute(const uint8_t *data, long long len, uint8_t out[16]) {
    uint32_t a0 = 0x67452301, b0 = 0xefcdab89;
    uint32_t c0 = 0x98badcfe, d0 = 0x10325476;

    /* Pre-processing: padding */
    long long new_len = ((len + 8) / 64 + 1) * 64;
    uint8_t *msg = (uint8_t *)calloc(new_len, 1);
    if (!msg) return;
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bits_len = (uint64_t)len * 8;
    memcpy(msg + new_len - 8, &bits_len, 8);

    for (long long offset = 0; offset < new_len; offset += 64) {
        uint32_t *M = (uint32_t *)(msg + offset);
        uint32_t A = a0, B = b0, C = c0, D = d0;
        for (int i = 0; i < 64; i++) {
            uint32_t F, g;
            if (i < 16) { F = (B & C) | (~B & D); g = i; }
            else if (i < 32) { F = (D & B) | (~D & C); g = (5*i+1) % 16; }
            else if (i < 48) { F = B ^ C ^ D; g = (3*i+5) % 16; }
            else { F = C ^ (B | ~D); g = (7*i) % 16; }
            F = F + A + _md5_K[i] + M[g];
            A = D; D = C; C = B;
            B = B + _md5_left_rotate(F, _md5_s[i]);
        }
        a0 += A; b0 += B; c0 += C; d0 += D;
    }
    free(msg);

    memcpy(out,      &a0, 4);
    memcpy(out + 4,  &b0, 4);
    memcpy(out + 8,  &c0, 4);
    memcpy(out + 12, &d0, 4);
}

TrMetin _tr_md5(const char *ptr, long long len) {
    uint8_t hash[16];
    _md5_compute((const uint8_t *)ptr, len, hash);
    char *hex = (char *)malloc(33);
    TrMetin m = {hex, 32};
    if (!hex) { m.len = 0; return m; }
    static const char hextab[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        hex[i*2]   = hextab[hash[i] >> 4];
        hex[i*2+1] = hextab[hash[i] & 0xf];
    }
    return m;
}

/* --- SHA-256 (FIPS 180-4) --- */

static const uint32_t _sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t _sha256_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static uint32_t _sha256_ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static uint32_t _sha256_maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static uint32_t _sha256_ep0(uint32_t x) { return _sha256_rotr(x,2) ^ _sha256_rotr(x,13) ^ _sha256_rotr(x,22); }
static uint32_t _sha256_ep1(uint32_t x) { return _sha256_rotr(x,6) ^ _sha256_rotr(x,11) ^ _sha256_rotr(x,25); }
static uint32_t _sha256_sig0(uint32_t x) { return _sha256_rotr(x,7) ^ _sha256_rotr(x,18) ^ (x >> 3); }
static uint32_t _sha256_sig1(uint32_t x) { return _sha256_rotr(x,17) ^ _sha256_rotr(x,19) ^ (x >> 10); }

static void _sha256_compute(const uint8_t *data, long long len, uint8_t out[32]) {
    uint32_t h0=0x6a09e667, h1=0xbb67ae85, h2=0x3c6ef372, h3=0xa54ff53a;
    uint32_t h4=0x510e527f, h5=0x9b05688c, h6=0x1f83d9ab, h7=0x5be0cd19;

    long long new_len = ((len + 8) / 64 + 1) * 64;
    uint8_t *msg = (uint8_t *)calloc(new_len, 1);
    if (!msg) return;
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    /* Big-endian length */
    for (int i = 0; i < 8; i++)
        msg[new_len - 1 - i] = (uint8_t)(bits >> (i * 8));

    for (long long offset = 0; offset < new_len; offset += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)msg[offset+i*4] << 24) |
                    ((uint32_t)msg[offset+i*4+1] << 16) |
                    ((uint32_t)msg[offset+i*4+2] << 8) |
                    ((uint32_t)msg[offset+i*4+3]);
        }
        for (int i = 16; i < 64; i++)
            w[i] = _sha256_sig1(w[i-2]) + w[i-7] + _sha256_sig0(w[i-15]) + w[i-16];

        uint32_t a=h0, b=h1, c=h2, d=h3, e=h4, f=h5, g=h6, h=h7;
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = h + _sha256_ep1(e) + _sha256_ch(e,f,g) + _sha256_k[i] + w[i];
            uint32_t t2 = _sha256_ep0(a) + _sha256_maj(a,b,c);
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h0+=a; h1+=b; h2+=c; h3+=d; h4+=e; h5+=f; h6+=g; h7+=h;
    }
    free(msg);

    uint32_t hash[8] = {h0,h1,h2,h3,h4,h5,h6,h7};
    for (int i = 0; i < 8; i++) {
        out[i*4]   = (hash[i] >> 24) & 0xff;
        out[i*4+1] = (hash[i] >> 16) & 0xff;
        out[i*4+2] = (hash[i] >> 8) & 0xff;
        out[i*4+3] = hash[i] & 0xff;
    }
}

TrMetin _tr_sha256(const char *ptr, long long len) {
    uint8_t hash[32];
    _sha256_compute((const uint8_t *)ptr, len, hash);
    char *hex = (char *)malloc(65);
    TrMetin m = {hex, 64};
    if (!hex) { m.len = 0; return m; }
    static const char hextab[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        hex[i*2]   = hextab[hash[i] >> 4];
        hex[i*2+1] = hextab[hash[i] & 0xf];
    }
    return m;
}

/* --- Base64 --- */

static const char _b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

TrMetin _tr_base64_kodla(const char *ptr, long long len) {
    long long out_len = 4 * ((len + 2) / 3);
    char *out = (char *)malloc(out_len);
    TrMetin m = {out, out_len};
    if (!out) { m.len = 0; return m; }

    long long j = 0;
    for (long long i = 0; i < len; i += 3) {
        uint32_t octet_a = (uint8_t)ptr[i];
        uint32_t octet_b = (i+1 < len) ? (uint8_t)ptr[i+1] : 0;
        uint32_t octet_c = (i+2 < len) ? (uint8_t)ptr[i+2] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        out[j++] = _b64_table[(triple >> 18) & 0x3f];
        out[j++] = _b64_table[(triple >> 12) & 0x3f];
        out[j++] = (i+1 < len) ? _b64_table[(triple >> 6) & 0x3f] : '=';
        out[j++] = (i+2 < len) ? _b64_table[triple & 0x3f] : '=';
    }
    return m;
}

static int _b64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

TrMetin _tr_base64_coz(const char *ptr, long long len) {
    TrMetin m = {NULL, 0};
    if (len <= 0 || len % 4 != 0) return m;
    long long out_len = (len / 4) * 3;
    if (ptr[len-1] == '=') out_len--;
    if (len > 1 && ptr[len-2] == '=') out_len--;
    if (out_len <= 0) return m;

    char *out = (char *)malloc((size_t)out_len);
    if (!out) return m;

    long long j = 0;
    for (long long i = 0; i < len; i += 4) {
        int a = _b64_decode_char(ptr[i]);
        int b = _b64_decode_char(ptr[i+1]);
        int c = (ptr[i+2] != '=') ? _b64_decode_char(ptr[i+2]) : 0;
        int d = (ptr[i+3] != '=') ? _b64_decode_char(ptr[i+3]) : 0;
        if (a < 0 || b < 0) { free(out); return m; }
        uint32_t triple = (a << 18) | (b << 12) | (c << 6) | d;
        if (j < out_len) out[j++] = (triple >> 16) & 0xff;
        if (j < out_len) out[j++] = (triple >> 8) & 0xff;
        if (j < out_len) out[j++] = triple & 0xff;
    }
    m.ptr = out;
    m.len = out_len;
    return m;
}
