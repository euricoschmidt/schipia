#ifndef SCHIPIA_MEMORY_STORE_H
#define SCHIPIA_MEMORY_STORE_H
#include <stddef.h>
#include <stdint.h>

#define MEMORY_STORE_MAX_KEY 256
/* Exclusive process ownership; keys are binary, 1..256 bytes. No shared handles.
 * put is insert-only (EEXIST on duplicates). Values include 0 and UINT64_MAX.
 * get: 0 success, -1 error (ENOENT on absence). close synchronizes storage.
 * File format is native-endian v1, for the same architecture/libc only.
 * No atomic transaction across the data and tree files; rebuild after crashes.
 */
void *memory_store_open(const char *path, size_t initial_size);
int memory_store_put(void *store, const void *key, size_t key_len, uint64_t recid);
int memory_store_get(void *store, const void *key, size_t key_len, uint64_t *out_recid);
int memory_store_sync(void *store);
void memory_store_close(void *store);
#endif
