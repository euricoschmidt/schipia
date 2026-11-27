/*
 * Exemplo 02 — Backends GPU zero-dependência (CUDA via libcuda.so.1 e Vulkan via libvulkan.so.1).
 * Execute com:
 *   SCHIPIA_GPU=cuda ./02_gpu_backends
 *   SCHIPIA_GPU=vulkan ./02_gpu_backends
 *   SCHIPIA_GPU=vulkan SCHIPIA_VK_DEVICE=0 ./02_gpu_backends
 */
#include <kernels.h>
#include <kernels_cuda.h>
#include <kernels_vulkan.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  int has_cu = sct_cuda_enabled();
  int has_vk = sct_vulkan_enabled();
  printf("GPU status: cuda=%d vulkan=%d (defina SCHIPIA_GPU=1|cuda|vulkan)\n",
         has_cu, has_vk);
  if (!has_cu && !has_vk) {
    puts("OK: modo CPU ativo (nenhum backend GPU solicitado ou disponivel).");
    return 0;
  }

  const size_t rows = 512, cols = 512, batch = 4;
  float *w_f32 = malloc(rows * cols * sizeof(float));
  float *x = malloc(batch * cols * sizeof(float));
  float *y_cpu = malloc(batch * rows * sizeof(float));
  float *y_gpu = malloc(batch * rows * sizeof(float));
  uint8_t *w_q4p = malloc(sct_q4p_matrix_bytes(rows, cols));
  if (!w_f32 || !x || !y_cpu || !y_gpu || !w_q4p) return 1;

  for (size_t i = 0; i < rows * cols; i++)
    w_f32[i] = 0.05f * sinf((float)(i % 997) * 0.11f);
  for (size_t i = 0; i < batch * cols; i++)
    x[i] = 0.4f * cosf((float)(i % 311) * 0.19f);
  sct_q4p_pack_matrix(w_f32, w_q4p, rows, cols);

  for (size_t t = 0; t < batch; t++)
    sct_matvec_rows_q4p_scalar(w_q4p, x + t * cols, y_cpu + t * rows, rows, cols);

  int rc = 0;
  if (has_cu) {
    if (sct_cuda_matvec_q4p_batch(w_q4p, x, y_gpu, batch, rows, cols, cols, rows) != 0)
      return 2;
    double maxd = 0.0;
    for (size_t i = 0; i < batch * rows; i++) {
      double d = fabs((double)y_cpu[i] - (double)y_gpu[i]);
      if (d > maxd) maxd = d;
    }
    printf("OK: CUDA Q4P 512x512 (m=%zu) max_diff=%.3e\n", batch, maxd);
    if (maxd > 1e-1) rc = 3;
  }
  if (has_vk) {
    if (sct_vulkan_matvec_q4p_batch(w_q4p, x, y_gpu, batch, rows, cols, cols, rows) != 0)
      return 4;
    double maxd = 0.0;
    for (size_t i = 0; i < batch * rows; i++) {
      double d = fabs((double)y_cpu[i] - (double)y_gpu[i]);
      if (d > maxd) maxd = d;
    }
    printf("OK: Vulkan Q4P 512x512 (m=%zu) max_diff=%.3e\n", batch, maxd);
    if (maxd > 1e-1) rc = 5;
  }

  free(w_q4p); free(y_gpu); free(y_cpu); free(x); free(w_f32);
  return rc;
}
