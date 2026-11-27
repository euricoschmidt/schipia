/*
 * Exemplo 05 — Geração completa com modelo Qwen2/Qwen3/MoE (.qw ou .qwt) e vocabulário.
 * Uso:
 *   ./05_qwen_generate <modelo.qw> <vocab.qw> "Sua pergunta aqui" [max_tokens]
 */
#include <kernels.h>
#include <qwen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc < 4) {
    printf("Uso: %s <modelo.qw> <vocab.qw> <prompt> [max_tokens]\n", argv[0]);
    puts("OK: exemplo pronto para execucao com qualquer modelo Qwen2/Qwen3/MoE em Q4P/Q4/Q8/BF16.");
    return 0;
  }
  sct_set_num_threads(0);
  int max_tokens = argc >= 5 ? atoi(argv[4]) : 48;
  if (max_tokens < 1) max_tokens = 1;

  qw_tokenizer *tok = qw_tokenizer_open(argv[2]);
  qw_model *m = qw_open(argv[1], 4096);
  if (!tok || !m) {
    perror("abrir modelo/vocabulario");
    qw_close(m);
    qw_tokenizer_close(tok);
    return 1;
  }
  uint32_t ids[2048];
  int n = qw_encode(tok, argv[3], strlen(argv[3]), ids, 2048, 1);
  if (n <= 0 || qw_prefill_cached(m, ids, (size_t)n, 1) != 0) {
    perror("prefill");
    qw_close(m);
    qw_tokenizer_close(tok);
    return 2;
  }
  unsigned vocab = qw_vocabulary(m);
  for (int step = 0; step < max_tokens; step++) {
    const float *logits = qw_logits(m);
    uint32_t best = 0;
    for (unsigned i = 1; i < vocab; i++)
      if (logits[i] > logits[best]) best = i;
    size_t len = 0;
    const char *piece = qw_piece(tok, best, &len);
    if (piece && len) fwrite(piece, 1, len, stdout);
    fflush(stdout);
    if (step + 1 < max_tokens && qw_forward(m, best, 1) != 0) break;
  }
  putchar('\n');
  qw_close(m);
  qw_tokenizer_close(tok);
  return 0;
}
