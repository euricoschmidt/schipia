#ifndef SCHIPIA_SEMANTIC_H
#define SCHIPIA_SEMANTIC_H
#include "helpdesk.h"
#include "qwen.h"
typedef struct hd_semantic hd_semantic;
int hd_semantic_build(hd_kb *, const char *model, const char *vocab,
                      const char *directory);
hd_semantic *hd_semantic_open(const char *model, const char *vocab,
                              const char *directory);
void hd_semantic_close(hd_semantic *);
int hd_semantic_context_query(hd_semantic *,hd_kb *,const hd_context *,const char *,int contextual,hd_result *);
int hd_semantic_query(hd_semantic *, hd_kb *, const hd_context *, const char *,
                      hd_result *);
#endif
