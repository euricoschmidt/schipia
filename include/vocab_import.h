#ifndef SCHIPIA_VOCAB_IMPORT_H
#define SCHIPIA_VOCAB_IMPORT_H
#include "tokenizer.h"
enum sct_vocab_format { SCT_VOCAB_JSON, SCT_VOCAB_SENTENCEPIECE, SCT_VOCAB_LINES };
/* Input must remain mapped for the call. JSON requires a readable NUL sentinel
 * at input[size] (outside the content). stride=0 leaves embedding offsets
 * unbound; otherwise offset=id*stride, checked for overflow/reserved UINT64_MAX.
 * On failure error_offset identifies the offending input region. No allocations
 * proportional to vocabulary size; caller must discard the failed builder.
 */
int sct_vocab_import(sct_vocab *, const void *, size_t, enum sct_vocab_format,
                     uint64_t stride, size_t *error_offset);
/* Exact pinned Tucano-160M tokenizer profile; rejects other schemas. */
int sct_vocab_import_tucano(sct_vocab *,const void *,size_t,uint64_t stride);
#endif
