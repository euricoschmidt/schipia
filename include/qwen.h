#ifndef SCHIPIA_QWEN_H
#define SCHIPIA_QWEN_H
#include <stddef.h>
#include <stdint.h>
typedef struct qw_model qw_model;
typedef struct qw_tokenizer qw_tokenizer;
/* Native C runtime. Original safetensors are read only during conversion.
 * Each runtime owns its KV cache; weights and vocabulary remain mmap-backed. */
int qw_import(const char *source, const char *destination, int bits);
/* Lossless tree bundle: destination must not exist. */
int qw_pack_tree(const char *source, const char *destination);
int qw_quantize_tree_q4(const char *source, const char *destination);
int qw_quantize_tree_q4p(const char *source, const char *destination);
int qw_verify_tree(const char *path);
/* Compatibility fingerprint for semantic indexes, not an authenticity hash. */
uint64_t qw_signature(const char *path);
qw_model *qw_open(const char *path, unsigned context);
int qw_enable_kv_cache(qw_model *model, const char *directory);
void qw_close(qw_model *model);
void qw_reset(qw_model *model);
unsigned qw_dimension(const qw_model *model);
unsigned qw_vocabulary(const qw_model *model);
unsigned qw_position(const qw_model *model);
void qw_truncate(qw_model *model, unsigned position);
unsigned qw_cached_tokens(const qw_model *model);
int qw_forward(qw_model *model, uint32_t token, int logits);
int qw_prefill(qw_model *model, const uint32_t *tokens, size_t count,
               int logits);
int qw_prefill_logits(qw_model *model, const uint32_t *tokens, size_t count,
                      float *out_logits);
size_t qw_draft_ngram(const qw_model *model, uint32_t last_token,
                      uint32_t *draft, size_t max_draft);
int qw_prefill_cached(qw_model *model, const uint32_t *tokens, size_t count,
                      int logits);
const float *qw_logits(const qw_model *model);
const float *qw_embedding(const qw_model *model);
qw_tokenizer *qw_tokenizer_open(const char *path);
void qw_tokenizer_close(qw_tokenizer *tokenizer);
int qw_encode(qw_tokenizer *tokenizer, const char *text, size_t length,
              uint32_t *tokens, size_t capacity, int special);
const char *qw_piece(qw_tokenizer *tokenizer, uint32_t token, size_t *length);
int qw_tokenizer_import(const char *json, const char *output);
#endif
