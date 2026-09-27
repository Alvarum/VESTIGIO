#include "assets/import/sha256.h"

#include <string.h>

#define ROR32(value, bits) (((value) >> (bits)) | ((value) << (32u - (bits))))

static const uint32_t k_constants[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u};

static uint32_t load_be32(const uint8_t *data) {
    return ((uint32_t)data[0] << 24u) | ((uint32_t)data[1] << 16u) | ((uint32_t)data[2] << 8u) |
           (uint32_t)data[3];
}

static void store_be32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)(value >> 24u);
    data[1] = (uint8_t)(value >> 16u);
    data[2] = (uint8_t)(value >> 8u);
    data[3] = (uint8_t)value;
}

static void transform(VgSha256 *hash, const uint8_t block[64]) {
    uint32_t words[64];
    uint32_t a, b, c, d, e, f, g, h;
    size_t index;

    for (index = 0; index < 16u; ++index) {
        words[index] = load_be32(block + index * 4u);
    }
    for (; index < 64u; ++index) {
        const uint32_t s0 = ROR32(words[index - 15u], 7u) ^ ROR32(words[index - 15u], 18u) ^
                            (words[index - 15u] >> 3u);
        const uint32_t s1 = ROR32(words[index - 2u], 17u) ^ ROR32(words[index - 2u], 19u) ^
                            (words[index - 2u] >> 10u);
        words[index] = words[index - 16u] + s0 + words[index - 7u] + s1;
    }

    a = hash->state[0];
    b = hash->state[1];
    c = hash->state[2];
    d = hash->state[3];
    e = hash->state[4];
    f = hash->state[5];
    g = hash->state[6];
    h = hash->state[7];
    for (index = 0; index < 64u; ++index) {
        const uint32_t s1 = ROR32(e, 6u) ^ ROR32(e, 11u) ^ ROR32(e, 25u);
        const uint32_t choice = (e & f) ^ ((~e) & g);
        const uint32_t t1 = h + s1 + choice + k_constants[index] + words[index];
        const uint32_t s0 = ROR32(a, 2u) ^ ROR32(a, 13u) ^ ROR32(a, 22u);
        const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t t2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    hash->state[0] += a;
    hash->state[1] += b;
    hash->state[2] += c;
    hash->state[3] += d;
    hash->state[4] += e;
    hash->state[5] += f;
    hash->state[6] += g;
    hash->state[7] += h;
}

void vg_sha256_init(VgSha256 *hash) {
    static const uint32_t initial[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    memcpy(hash->state, initial, sizeof(initial));
    hash->bit_count = 0u;
    hash->block_size = 0u;
}

void vg_sha256_update(VgSha256 *hash, const void *data_value, size_t size) {
    const uint8_t *data = (const uint8_t *)data_value;
    while (size != 0u) {
        size_t count = 64u - hash->block_size;
        if (count > size)
            count = size;
        memcpy(hash->block + hash->block_size, data, count);
        hash->block_size += count;
        hash->bit_count += (uint64_t)count * 8u;
        data += count;
        size -= count;
        if (hash->block_size == 64u) {
            transform(hash, hash->block);
            hash->block_size = 0u;
        }
    }
}

void vg_sha256_finish(VgSha256 *hash, uint8_t digest[32]) {
    const uint64_t bits = hash->bit_count;
    size_t index;
    hash->block[hash->block_size++] = 0x80u;
    if (hash->block_size > 56u) {
        memset(hash->block + hash->block_size, 0, 64u - hash->block_size);
        transform(hash, hash->block);
        hash->block_size = 0u;
    }
    memset(hash->block + hash->block_size, 0, 56u - hash->block_size);
    for (index = 0; index < 8u; ++index) {
        hash->block[63u - index] = (uint8_t)(bits >> (index * 8u));
    }
    transform(hash, hash->block);
    for (index = 0; index < 8u; ++index)
        store_be32(digest + index * 4u, hash->state[index]);
    memset(hash, 0, sizeof(*hash));
}
