/*
 * Exemplo 03 — Memória factual persistente (libschiptree) e alocação alinhada.
 * Armazena e recupera registros binários com persistência em disco.
 */
#include <schipia.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  char base[128];
  snprintf(base, sizeof(base), "/tmp/schipia_ex_mem_%ld", (long)getpid());
  char dfile[160], tfile[160];
  snprintf(dfile, sizeof(dfile), "%s.dat", base);
  snprintf(tfile, sizeof(tfile), "%s.tree", base);
  unlink(dfile);
  unlink(tfile);

  void *store = memory_store_open(base, 4096);
  if (!store) {
    perror("memory_store_open");
    return 1;
  }
  const char *k1 = "cliente:empresa-a:perfil";
  const char *k2 = "sessao:42:ultimo_token";
  if (memory_store_put(store, k1, strlen(k1), 1001) != 0 ||
      memory_store_put(store, k2, strlen(k2), 987654321ULL) != 0 ||
      memory_store_sync(store) != 0) {
    memory_store_close(store);
    return 2;
  }
  memory_store_close(store);

  /* Reabre para confirmar persistência em disco */
  store = memory_store_open(base, 4096);
  if (!store) return 3;
  uint64_t v1 = 0, v2 = 0;
  int ok = (memory_store_get(store, k1, strlen(k1), &v1) == 0 && v1 == 1001 &&
            memory_store_get(store, k2, strlen(k2), &v2) == 0 && v2 == 987654321ULL);
  memory_store_close(store);
  unlink(dfile);
  unlink(tfile);

  float *buf = sct_buffer_alloc(256);
  if (!buf) return 4;
  free(buf);

  if (!ok) return 5;
  printf("OK: memoria persistente libschiptree validada (v1=%llu v2=%llu)\n",
         (unsigned long long)v1, (unsigned long long)v2);
  return 0;
}
