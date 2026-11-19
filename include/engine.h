#ifndef SCHIPIA_ENGINE_H
#define SCHIPIA_ENGINE_H
#include "tokenizer.h"
#include "transformer.h"

typedef struct sct_engine sct_engine;
enum { SCT_ENGINE_NEW_SEQUENCE=2 };
#define SCT_NO_EOS UINT64_MAX
typedef struct {
    uint64_t model_open_ns,vocab_open_ns,cache_open_ns,initialization_ns;
    sct_infer_stats inference;
} sct_engine_metrics;

/* Exclusive demo bundle: model.sct_model and a dense 256-token byte vocabulary.
 * Synthetic weights only; no trained-language-model claim.
 */
int sct_engine_demo(const char *directory,uint64_t seed);
sct_engine *sct_engine_open(const char *model,const char *vocab,const char *cache,
                            uint64_t sequence,int create_cache,unsigned flags);
const sct_model_config *sct_engine_configuration(const sct_engine *);
int sct_engine_trained(const sct_engine *);
/* Two-pass preflight: unknown bytes/IDs/embedding offsets and prompt+reserve
 * capacity are checked before any forward. Input must remain immutable.
 * error_offset is in bytes; prompt_tokens reports preflight count.
 */
int sct_engine_prefill(sct_engine *,const void *,size_t,size_t reserve,
                       size_t *prompt_tokens,size_t *error_offset);
typedef struct {
    float temperature;        /* <= 0.0f = greedy argmax, > 0.0f = sampling */
    float top_p;              /* 0.0f < top_p <= 1.0f (1.0f = disabled) */
    uint32_t top_k;           /* 0 = disabled */
    float repetition_penalty; /* 1.0f = disabled, e.g. 1.15f */
    uint64_t seed;            /* 0 = auto-seed */
} sct_sample_config;

/* Greedy decoding, ties select lowest ID; EOS is included and persisted.
 * No output callback: checkpoint before rendering durable IDs/pieces.
 */
int sct_engine_generate(sct_engine *,size_t maximum,uint64_t eos,
                        uint64_t *start,size_t *generated);
int sct_engine_generate_sample(sct_engine *,size_t maximum,uint64_t eos,
                               const sct_sample_config *sample,
                               uint64_t *start,size_t *generated);

int sct_engine_state(sct_engine *,uint64_t *tokens,const float **logits);
int sct_engine_piece(sct_engine *,uint32_t id,sct_token *);
int sct_engine_token(sct_engine *,uint64_t position,sct_token *);
int sct_engine_decoded_token(sct_engine *,uint64_t position,sct_token *);
sct_engine_metrics sct_engine_measurements(const sct_engine *);
int sct_engine_set_sparsity(sct_engine *,float threshold);
float sct_engine_get_sparsity(const sct_engine *);
int sct_engine_new_sequence(sct_engine *,uint64_t sequence);
int sct_engine_embed(sct_engine *,const void *text,size_t size,float *out,size_t dim);
int sct_engine_set_json_mode(sct_engine *,int enabled);
int sct_engine_checkpoint(sct_engine *);

void sct_engine_close(sct_engine *);
void sct_engine_abort(sct_engine *);
#endif
