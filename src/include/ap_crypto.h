#ifndef __AP_CRYPTO_H
#define __AP_CRYPTO_H

#include <stdint.h>

uint64_t ap_hash_djb2(const char *buf, int size);
uint64_t ap_hash_crc64(const char *buf, int size);

#endif /* __AP_CRYPTO_H */
