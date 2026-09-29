#pragma once
#include <cstddef>
#include <cstdint>
typedef struct { uint64_t state; uint64_t length; } mbedtls_sha256_context;
void mbedtls_sha256_init(mbedtls_sha256_context*);
void mbedtls_sha256_free(mbedtls_sha256_context*);
int mbedtls_sha256_starts_ret(mbedtls_sha256_context*, int);
int mbedtls_sha256_update_ret(mbedtls_sha256_context*, const unsigned char*, std::size_t);
int mbedtls_sha256_finish_ret(mbedtls_sha256_context*, unsigned char[32]);
int mbedtls_sha256_ret(const unsigned char*, std::size_t, unsigned char[32], int);
