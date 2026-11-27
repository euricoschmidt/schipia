/*
 * Exemplo 01 — Kernels Q4P, RMSNorm, Softmax, SwiGLU e threads dinâmicas.
 * Demonstra empacotamento Q4P (4x4 intercalado + MSE), matvec single/batch
 * e detecção automática de backend CPU e threads (cgroup v2 + núcleos online).
 */
#include <schipia.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  int threads = sct_auto_threads();
  sct_set_num_threads(0);
  printf("SchipIA %s | backend=%s | auto_threads=%d\n",
         SCHIPIA_VERSION_STRING, sct_backend_name(sct_kernel_backend()),
         threads);

  const size_t rows = 128, cols = 128, batch = 4;
  float *w_f32 = malloc(rows * cols * sizeof(float));
  float *x = malloc(batch * cols * sizeof(float));
  float *y_single = malloc(rows * sizeof(float));
  float *y_batch = malloc(batch * rows * sizeof(float));
  float *y_ref = malloc(rows * sizeof(float));
  uint8_t *w_q4p = malloc(sct_q4p_matrix_bytes(rows, cols));
  if (!w_f32 || !x || !y_single || !y_batch || !y_ref || !w_q4p) return 1;

  for (size_t i = 0; i < rows * cols; i++)
    w_f32[i] = 0.08f * sinf((float)(i + 1) * 0.13f);
  for (size_t i = 0; i < batch * cols; i++)
    x[i] = 0.5f * cosf((float)(i + 1) * 0.17f);

  sct_q4p_pack_matrix(w_f32, w_q4p, rows, cols);
  if (sct_matvec_rows_q4p_scalar(w_q4p, x, y_ref, rows, cols) != 0 ||
      sct_matvec_rows_q4p(w_q4p, x, y_single, rows, cols) != 0 ||
      sct_matvec_rows_q4p_batch(w_q4p, x, y_batch, batch, rows, cols, cols, rows) != 0)
    return 2;

  double max_diff = 0.0;
  for (size_t r = 0; r < rows; r++) {
    double d = fabs((double)y_single[r] - (double)y_batch[r]);
    if (d > max_diff) max_diff = d;
  }

  /* RMSNorm + Softmax + SwiGLU */
  float norm_w[128], out_n[128], probs[128], sw[128];
  for (size_t i = 0; i < 128; i++) norm_w[i] = 1.0f;
  if (sct_rmsnorm(x, norm_w, out_n, 128, 1e-6f) != 0 ||
      sct_softmax(out_n, probs, 128) != 0 ||
      sct_swiglu(out_n, probs, sw, 128) != 0)
    return 3;

  double psum = 0.0;
  for (size_t i = 0; i < 128; i++) psum += probs[i];
  printf("OK: Q4P single vs batch max_diff=%.3e | softmax_sum=%.6f | swiglu[0]=%.6f\n",
         max_diff, psum, sw[0]);

  free(w_q4p); free(y_ref); free(y_batch); free(y_single); free(x); free(w_f32);
  return (max_diff < 1e-2 && fabs(psum - 1.0) < 1e-4) ? 0 : 4;
}
