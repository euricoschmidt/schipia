#ifndef SCHIPIA_TRANSFORMER_H
#define SCHIPIA_TRANSFORMER_H
#include <stddef.h>
#include <stdint.h>

#define SCT_KV_BLOCK_TOKENS 8u
typedef struct sct_model sct_model;
typedef struct sct_context sct_context;
typedef struct { uint32_t dim, hidden, heads, layers, vocab, max_context; } sct_model_config;
enum sct_tensor {
    SCT_EMBEDDING, SCT_FINAL_NORM, SCT_LM_HEAD,
    SCT_ATT_NORM, SCT_WQ, SCT_WK, SCT_WV, SCT_WO, SCT_FF_NORM, SCT_UP, SCT_GATE, SCT_DOWN
};
enum { SCT_INFER_SCALAR = 1, SCT_INFER_NO_HOT = 4, SCT_INFER_SPARSE = 8, SCT_INFER_OUT_OF_CORE = 16 };
typedef struct {
    uint64_t forward_steps, new_kv_blocks, attention_block_reads, index_lookups, hot_hits, hot_bytes;
    uint64_t sparse_clusters_total, sparse_clusters_active, sparse_bytes_saved;
    uint64_t prefix_cache_hits, tree_lookups, dedup_saved_bytes;
} sct_infer_stats;

/* Creates an exclusive native mmap file with deterministic synthetic weights.
 * This is a test model generator, not trained language-model weights.
 * Files are immutable while mapped; native format requires compatible ABI.
 */
int sct_model_create(const char *path, const sct_model_config *, uint64_t seed);
/* Import the original llama2.c FP32 checkpoint (equal Q/KV head counts).
 * Input/output are mmap; dense projections are transposed once at import.
 * A tied classifier shares the embedding tensor on disk.
 */
int sct_model_import_llama(const char *source,const char *output);
/* Rewrite a float32 model as bfloat16 (format v3), half the size. Lossless by
 * construction: fails with ERANGE if any weight has nonzero low 16 bits.
 * Matrices become [out][in] row-major; normalization weights stay float32.
 * The packed model has its own identity, so caches are not shared with the source.
 */
int sct_model_pack_bf16(const char *source,const char *output);
int sct_model_bf16(const sct_model *);
/* Rewrite a float32 or bfloat16 model with 8-bit weights (32-weight blocks, one
 * 16-bit scale each, 8.5 bits per weight). Lossy: logits change slightly and
 * the model gets its own identity. Normalization weights stay float32.
 */
int sct_model_pack_q8(const char *source,const char *output);
int sct_model_q8(const sct_model *);
/* Rewrite a float32, bfloat16 or Q8 model with 4-bit weights (32-weight blocks, one
 * 16-bit scale each, 4.5 bits per weight). Lossy: weights are quantized to 4 bits
 * (-8..+7). Normalization weights stay float32.
 */
int sct_model_pack_q4(const char *source,const char *output);
int sct_model_q4(const sct_model *);
int sct_model_trained(const sct_model *);
sct_model *sct_model_open(const char *path);
void sct_model_close(sct_model *);
const sct_model_config *sct_model_configuration(const sct_model *);
const float *sct_model_tensor(const sct_model *, uint32_t layer, enum sct_tensor, size_t *elements);

/* Cache is an exclusive directory with mmap state and a libschiptree index.
 * create=1 is exclusive; create=0 never creates missing files.
 * model is borrowed and must outlive context. Handles are single-threaded.
 * Only clean checkpoints reopen: interrupted/aborted writes return EUCLEAN.
 */
sct_context *sct_context_open(const sct_model *, const char *directory, int create, unsigned flags);
int sct_sequence_create(sct_context *, uint64_t sequence);
int sct_forward(sct_context *, uint64_t sequence, uint32_t token);
int sct_forward_batch(sct_context *, uint64_t sequence, const uint32_t *tokens, size_t count);
/* Read-only view; logits is NULL before the first token. Pointers remain valid
 * until the next mutation/remap or close. Does not run inference/replay.
 */
int sct_sequence_state(sct_context *, uint64_t, uint64_t *tokens, const float **logits);
int sct_sequence_token(sct_context *, uint64_t sequence, uint64_t position, uint32_t *token);
sct_infer_stats sct_context_stats(const sct_context *);
/* Write-once vocabulary identity; binding requires an entirely empty cache.
 * Existing nonempty unbound caches cannot be adopted by the text engine.
 */
int sct_context_bind_vocab(sct_context *,const uint64_t identity[2],int bind_empty);
int sct_context_sync(sct_context *);
/* Dynamic neuron activation sparsity (PowerInfer-style).
 * Threshold 0.0f (default) runs exact dense evaluation.
 * Values like 0.01f to 0.05f dynamically skip inactive neuron clusters in SwiGLU FFN.
 */
int sct_context_set_sparsity(sct_context *, float threshold);
float sct_context_get_sparsity(const sct_context *);

/* Index model weights into a libschiptree directory bundle with deduplication.
 * Creates DIRECTORY/weights.index and DIRECTORY/weights.bin (or model.sct_model).
 */
int sct_model_pack_tree(const char *source, const char *output_dir);
int sct_model_pack_tree_aligned(const char *source, const char *output_dir, int page_align);
int sct_model_is_tree(const sct_model *);
int sct_model_cluster_lookup(const sct_model *, uint32_t layer, enum sct_tensor, uint32_t cluster, uint64_t *offset);

void sct_context_close(sct_context *);
/* Releases resources without a checkpoint; changed caches remain dirty. */
void sct_context_abort(sct_context *);
#endif
