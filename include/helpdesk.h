#ifndef SCHIPIA_HELPDESK_H
#define SCHIPIA_HELPDESK_H
#include <stddef.h>
#include <stdint.h>
#define HD_BODY 4096
#define HD_QUESTION 2048
#define HD_RESULTS 3
typedef struct hd_kb hd_kb;
typedef struct hd_memory hd_memory;
typedef struct {
    char application[48],version[64],screen[96],role[32],tenant[64],user[96];
} hd_context;
typedef struct {
    char id[96],application[48],version[64],screen[96],roles[128],title[256],url[512],tenant[64];
    uint64_t body_offset;
    uint32_t body_length,words;
} hd_article;
typedef struct { hd_article article;const char *body;double score;unsigned matched,terms; } hd_hit;
typedef struct { hd_hit hits[HD_RESULTS];size_t count;int learned_alias; } hd_result;
enum { HD_DIALOGUE=1, HD_HELPFUL=2, HD_CONTRIBUTION=3, HD_APPROVE=4, HD_REJECT=5 };
typedef struct {
    uint64_t id,target,timestamp;
    uint32_t kind;
    hd_context context;
    char client[48],source[96],question[HD_QUESTION],text[HD_BODY],title[256];
} hd_event;
int hd_ingest(const char *manifest,const char *directory);
hd_kb *hd_open(const char *directory);
void hd_close(hd_kb *);
size_t hd_count(const hd_kb *);
int hd_at(hd_kb *,size_t,hd_hit *);
int hd_query(hd_kb *,const hd_context *,const char *question,const char *history,hd_result *);
int hd_explicit_operation(const char *question,const hd_hit *);
int hd_by_id(hd_kb *,const hd_context *,const char *,hd_hit *);
int hd_source(hd_kb *,const char *application,const char *versions,const char *role,const char *id,hd_hit *);
int hd_allowed(const hd_article *,const hd_context *);
int hd_match_list(const char *,const char *);
hd_memory *hd_memory_open(const char *directory);
void hd_memory_close(hd_memory *);
int hd_memory_append(hd_memory *,hd_event *);
const hd_event *hd_memory_get(hd_memory *,uint64_t);
size_t hd_memory_count(const hd_memory *);
const hd_event *hd_memory_at(const hd_memory *,size_t);
int hd_memory_decision(const hd_memory *,uint64_t);
int hd_memory_search(hd_memory *,hd_kb *,const hd_context *,const char *,hd_result *);
int hd_same_owner(const hd_event *,const hd_context *,const char *client);
#endif
