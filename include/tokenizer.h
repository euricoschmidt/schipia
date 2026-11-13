#ifndef SCHIPIA_TOKENIZER_H
#define SCHIPIA_TOKENIZER_H
#include <stddef.h>
#include <stdint.h>

#define SCT_TOKEN_MAX 256u
#define SCT_EMBEDDING_UNBOUND UINT64_MAX
enum { SCT_TOKEN_GREEDY=0,SCT_TOKEN_LLAMA_BPE=1,SCT_TOKEN_TUCANO_BPE=2,SCT_TOKEN_BOS=1,SCT_TOKEN_EOS=2 };
typedef struct { uint32_t left,right,result,rank; } sct_bpe_merge;
typedef struct sct_vocab sct_vocab;
typedef struct { uint32_t id; uint64_t embedding_offset; const unsigned char *bytes; size_t length; } sct_token;

/* A vocabulary is a directory containing mmap data and two libschiptree indices.
 * Creation is exclusive. Unfinished builders are removed on close.
 * finish seals immutable records; open rejects incomplete or incompatible files.
 * Handles are not thread-shareable; use separate opens. Index files must be writable.
 */
sct_vocab *sct_vocab_create(const char *directory);
sct_vocab *sct_vocab_open(const char *directory);
/* add: 0 inserted, 1 identical existing entry, -1 error. Conflicts: EEXIST.
 * Tokens are 1..256 binary bytes; IDs are unique uint32 values including zero.
 * add failure invalidates the builder: close it instead of continuing.
 */
int sct_vocab_add(sct_vocab *, const void *, size_t, uint32_t, uint64_t);
int sct_vocab_finish(sct_vocab *);
void sct_vocab_close(sct_vocab *);
uint64_t sct_vocab_count(const sct_vocab *);
/* Immutable semantic fingerprint and maximum ID. v2 reads header metadata;
 * legacy v1 scans mapped records once per handle, without a heap table.
 * Fingerprint is order-independent and non-cryptographic.
 */
int sct_vocab_metadata(sct_vocab *, uint64_t identity[2], uint32_t *max_id);
int sct_vocab_algorithm(const sct_vocab *);
int sct_vocab_import_llama(const char *source,const char *output,uint32_t vocabulary,uint64_t embedding_stride);
int sct_vocab_bpe_configure(sct_vocab *,const float *scores,size_t count);
int sct_vocab_rank_configure(sct_vocab *,sct_bpe_merge *,size_t count);
/* Decoded bytes may use per-handle scratch, valid until the next decode.
 * previous=UINT32_MAX denotes the beginning of the decoded sequence. */
int sct_vocab_decode(sct_vocab *,uint32_t previous,uint32_t id,sct_token *);
int sct_vocab_lookup(sct_vocab *, const void *, size_t, sct_token *);
int sct_vocab_by_id(sct_vocab *, uint32_t, sct_token *);
/* Longest matching prefix; ENOENT if no token matches. No heap allocation.
 * Returned byte pointers point into mmap and remain valid until close
 * (or the next add, for builders). No Unicode normalization or BPE merges.
 */
int sct_tokenizer_next(sct_vocab *, const void *, size_t, sct_token *);
/* On error, count/consumed report the successfully encoded prefix.
 * ENOSPC: output full; ENOENT: unknown input byte; empty input succeeds.
 */
int sct_tokenizer_encode(sct_vocab *, const void *, size_t, uint32_t *, size_t,
                         size_t *count, size_t *consumed);
/* llama2.c uses scored merges/dummy space and rejects NUL. Tucano uses ranked
 * pairs and its original space normalizer, accepts NUL and rejects invalid
 * UTF-8. BOS/EOS are explicit flags. BPE output is atomic on ENOSPC. */
int sct_tokenizer_encode_flags(sct_vocab *,const void *,size_t,uint32_t *,size_t,
                               size_t *,size_t *,unsigned flags);
#endif
