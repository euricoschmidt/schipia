#ifndef SCHIPIA_DIALOGUE_H
#define SCHIPIA_DIALOGUE_H
#include "semantic.h"
typedef struct hd_brain hd_brain;
hd_brain *hd_brain_open(const char *directory);
void hd_brain_close(hd_brain *);
void hd_brain_set_cancel(hd_brain *, int (*should_cancel)(void *), void *ctx);
int hd_brain_set_kv_cache(hd_brain *, const char *directory);
int hd_brain_large_coder(const hd_brain *);
const char *hd_brain_directory(const hd_brain *);
int hd_brain_complete(const hd_brain *);
typedef int (*hd_code_emit)(void *,const char *,size_t);
void hd_brain_set_stream(hd_brain *, hd_code_emit on_token, void *ctx);
int hd_brain_code_stream(hd_brain *,const hd_context *,const char *,const char *,int,hd_code_emit,void *,unsigned *);
int hd_brain_rewrite(hd_brain *,const hd_context *,const char *question,const char *history,char *output,size_t capacity);
int hd_grounded_commands(const hd_result *,const char *text);
int hd_brain_embedding_index(hd_brain *, hd_kb *, const char *directory);
hd_semantic *hd_brain_semantic(hd_brain *, const char *directory);
int hd_brain_answer(hd_brain *, const hd_context *, const char *question,
                    const char *history, const hd_result *, int coding,
                    char *output, size_t capacity, unsigned max_tokens);
#endif
