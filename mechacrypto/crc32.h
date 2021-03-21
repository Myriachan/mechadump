#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


uint32_t crc32(const void* data, size_t n_bytes, uint32_t crc);


#ifdef __cplusplus
}
#endif
