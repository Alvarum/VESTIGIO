#ifndef VESTIGIO_SHA256_H
#define VESTIGIO_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct VgSha256 {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t block[64];
    size_t block_size;
} VgSha256;

void vg_sha256_init(VgSha256 *hash);
void vg_sha256_update(VgSha256 *hash, const void *data, size_t size);
void vg_sha256_finish(VgSha256 *hash, uint8_t digest[32]);

#endif
