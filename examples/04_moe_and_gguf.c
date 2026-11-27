/*
 * Exemplo 04 — Pipeline completo MoE + GGUF v3 -> Q4P + LFRU + Cache KV persistente.
 * Cria um mini-modelo GGUF v3 qwen2moe (4 experts 3D + shared_expert + gate),
 * importa diretamente para Q4P alinhado a 4096 B/expert via qw_import_gguf(),
 * habilita mapa de calor LFRU e cache KV em disco, e valida forward vs prefill.
 */
#define _POSIX_C_SOURCE 200809L
#include <qwen.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void w_str(FILE *f, const char *s) {
  uint64_t n = strlen(s);
  fwrite(&n, 8, 1, f);
  fwrite(s, 1, n, f);
}
static void w_kv_str(FILE *f, const char *k, const char *v) {
  uint32_t t = 8;
  w_str(f, k); fwrite(&t, 4, 1, f); w_str(f, v);
}
static void w_kv_u32(FILE *f, const char *k, uint32_t v) {
  uint32_t t = 4;
  w_str(f, k); fwrite(&t, 4, 1, f); fwrite(&v, 4, 1, f);
}
static void w_kv_f32(FILE *f, const char *k, float v) {
  uint32_t t = 6;
  w_str(f, k); fwrite(&t, 4, 1, f); fwrite(&v, 4, 1, f);
}

int main(void) {
  const unsigned dim = 64, hidden = 64, heads = 4, kv_heads = 2, hd = 16;
  const unsigned vocab = 32, n_exp = 4, top_k = 2;
  char gguf[128], out_qw[128], heat[128], kv_dir[128];
  long pid = (long)getpid();
  snprintf(gguf, sizeof(gguf), "/tmp/schipia_ex_moe_%ld.gguf", pid);
  snprintf(out_qw, sizeof(out_qw), "/tmp/schipia_ex_moe_%ld.qw", pid);
  snprintf(heat, sizeof(heat), "/tmp/schipia_ex_heat_%ld.bin", pid);
  snprintf(kv_dir, sizeof(kv_dir), "/tmp/schipia_ex_kv_%ld", pid);

  FILE *gf = fopen(gguf, "wb");
  if (!gf) return 1;
  uint32_t magic = 0x46554747u, ver = 3;
  uint64_t nt = 19, nkv = 13;
  fwrite(&magic, 4, 1, gf); fwrite(&ver, 4, 1, gf);
  fwrite(&nt, 8, 1, gf); fwrite(&nkv, 8, 1, gf);
  w_kv_str(gf, "general.architecture", "qwen2moe");
  w_kv_u32(gf, "qwen2moe.embedding_length", dim);
  w_kv_u32(gf, "qwen2moe.feed_forward_length", hidden);
  w_kv_u32(gf, "qwen2moe.expert_feed_forward_length", hidden);
  w_kv_u32(gf, "qwen2moe.block_count", 1);
  w_kv_u32(gf, "qwen2moe.attention.head_count", heads);
  w_kv_u32(gf, "qwen2moe.attention.head_count_kv", kv_heads);
  w_kv_u32(gf, "qwen2moe.attention.key_length", hd);
  w_kv_f32(gf, "qwen2moe.attention.layer_norm_rms_epsilon", 1e-6f);
  w_kv_f32(gf, "qwen2moe.rope.freq_base", 1000000.0f);
  w_kv_u32(gf, "qwen2moe.vocab_size", vocab);
  w_kv_u32(gf, "qwen2moe.expert_count", n_exp);
  w_kv_u32(gf, "qwen2moe.expert_used_count", top_k);

  struct { const char *name; uint32_t nd; uint64_t d0, d1, d2; size_t nb; } ts[19] = {
    {"token_embd.weight", 2, dim, vocab, 1, (size_t)vocab * dim * 4},
    {"output_norm.weight", 1, dim, 1, 1, (size_t)dim * 4},
    {"blk.0.attn_norm.weight", 1, dim, 1, 1, (size_t)dim * 4},
    {"blk.0.ffn_norm.weight", 1, dim, 1, 1, (size_t)dim * 4},
    {"blk.0.attn_q.weight", 2, dim, heads * hd, 1, (size_t)heads * hd * dim * 4},
    {"blk.0.attn_k.weight", 2, dim, kv_heads * hd, 1, (size_t)kv_heads * hd * dim * 4},
    {"blk.0.attn_v.weight", 2, dim, kv_heads * hd, 1, (size_t)kv_heads * hd * dim * 4},
    {"blk.0.attn_q.bias", 1, heads * hd, 1, 1, (size_t)heads * hd * 4},
    {"blk.0.attn_k.bias", 1, kv_heads * hd, 1, 1, (size_t)kv_heads * hd * 4},
    {"blk.0.attn_v.bias", 1, kv_heads * hd, 1, 1, (size_t)kv_heads * hd * 4},
    {"blk.0.attn_output.weight", 2, heads * hd, dim, 1, (size_t)dim * heads * hd * 4},
    {"blk.0.ffn_gate_inp.weight", 2, dim, n_exp, 1, (size_t)n_exp * dim * 4},
    {"blk.0.ffn_gate_exps.weight", 3, dim, hidden, n_exp, (size_t)n_exp * hidden * dim * 4},
    {"blk.0.ffn_up_exps.weight", 3, dim, hidden, n_exp, (size_t)n_exp * hidden * dim * 4},
    {"blk.0.ffn_down_exps.weight", 3, hidden, dim, n_exp, (size_t)n_exp * dim * hidden * 4},
    {"blk.0.ffn_gate_shexp.weight", 2, dim, hidden, 1, (size_t)hidden * dim * 4},
    {"blk.0.ffn_up_shexp.weight", 2, dim, hidden, 1, (size_t)hidden * dim * 4},
    {"blk.0.ffn_down_shexp.weight", 2, hidden, dim, 1, (size_t)dim * hidden * 4},
    {"blk.0.ffn_gate_inp_shexp.weight", 2, dim, 1, 1, (size_t)dim * 4},
  };
  uint64_t off = 0;
  for (int i = 0; i < 19; i++) {
    w_str(gf, ts[i].name);
    fwrite(&ts[i].nd, 4, 1, gf);
    fwrite(&ts[i].d0, 8, 1, gf);
    if (ts[i].nd >= 2) fwrite(&ts[i].d1, 8, 1, gf);
    if (ts[i].nd >= 3) fwrite(&ts[i].d2, 8, 1, gf);
    uint32_t tt = 0;
    fwrite(&tt, 4, 1, gf);
    fwrite(&off, 8, 1, gf);
    off += ts[i].nb;
  }
  long pos = ftell(gf), pad = ((pos + 31) & ~31L) - pos;
  for (long p = 0; p < pad; p++) fputc(0, gf);
  float *buf = malloc(off);
  if (!buf) { fclose(gf); return 2; }
  for (size_t i = 0; i < off / 4; i++) buf[i] = 0.04f * sinf((float)(i + 1) * 0.13f);
  fwrite(buf, 1, off, gf);
  free(buf);
  fclose(gf);

  if (qw_import_gguf(gguf, out_qw, 5) != 0) return 3;
  setenv("SCHIPIA_STREAM_MMAP", "1", 1);
  qw_model *m = qw_open(out_qw, 64);
  if (!m) return 4;
  (void)qw_enable_moe_heat(m, heat);
  (void)qw_enable_kv_cache(m, kv_dir);

  uint32_t toks[4] = {1, 2, 3, 4};
  float lf[32], lp[32];
  for (int i = 0; i < 4; i++)
    if (qw_forward(m, toks[i], i == 3) != 0) return 5;
  memcpy(lf, qw_logits(m), vocab * sizeof(float));

  qw_reset(m);
  if (qw_prefill_cached(m, toks, 4, 1) != 0) return 6;
  memcpy(lp, qw_logits(m), vocab * sizeof(float));
  qw_close(m);

  double maxd = 0.0;
  for (unsigned i = 0; i < vocab; i++) {
    double d = fabs((double)lf[i] - (double)lp[i]);
    if (d > maxd) maxd = d;
  }
  unlink(gguf); unlink(out_qw); unlink(heat);
  char cmd[200];
  snprintf(cmd, sizeof(cmd), "rm -rf %s", kv_dir);
  (void)system(cmd);

  printf("OK: GGUF v3 MoE (4 experts + shared_expert) -> Q4P | fwd_vs_prefill_max_diff=%.3e\n", maxd);
  return maxd < 1e-2 ? 0 : 7;
}
