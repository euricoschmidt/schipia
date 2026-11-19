#ifndef SCHIPIA_KERNELS_H
#define SCHIPIA_KERNELS_H
#include <stddef.h>
#include <stdint.h>

enum sct_backend { SCT_SCALAR, SCT_AVX2_FMA, SCT_NEON };
enum sct_backend sct_kernel_backend(void);
const char *sct_backend_name(enum sct_backend);
int sct_backend_available(enum sct_backend);
/* Scratch allocation aligned to 64 bytes; release with free(). */
float *sct_buffer_alloc(size_t elements);

/* Row-major contiguous matrices: C[M,N] += A[M,K] * B[K,N].
 * No allocation/packing. C must not overlap A/B. Zero dimensions are no-ops.
 * Arbitrary float alignment is supported; 64-byte alignment is recommended.
 * Explicit SIMD entry points return ENOTSUP on unavailable hardware.
 * Arithmetic follows float/FMA semantics; GEMM does not scan for NaNs/overflow.
 */
int sct_gemm(const float *a, const float *b, float *c, size_t m, size_t n, size_t k);
int gemm_scalar(const float *, const float *, float *, size_t, size_t, size_t);
int gemm_avx2(const float *, const float *, float *, size_t, size_t, size_t);
int gemm_neon(const float *, const float *, float *, size_t, size_t, size_t);
/* y = row_major_weights[rows,columns] * x[columns], overwrite y. */
int sct_matvec_rows(const float *,const float *,float *,size_t rows,size_t columns);
int sct_matvec_rows_scalar(const float *,const float *,float *,size_t,size_t);
/* Same product with bfloat16 weights: each uint16 is the upper half of an
 * IEEE float32 (lower 16 bits zero), expanded exactly while streaming.
 * Halves the bytes read per row; every output is an independent dot product,
 * so results do not depend on the number of threads.
 */
int sct_matvec_rows_bf16(const uint16_t *,const float *,float *,size_t rows,size_t columns);
int sct_matvec_rows_bf16_scalar(const uint16_t *,const float *,float *,size_t,size_t);
void sct_bf16_expand(const uint16_t *,float *,size_t);
/* Q8: lossy 8-bit weights. A row is ceil(columns/32) blocks of
 * [uint16 scale][32 x int8]; the scale is the upper half of a float32 and
 * weight = scale * q. The last block is zero padded. The product is integer:
 * the activation vector is rounded to int16 against its largest magnitude once
 * per call (relative step 2^-15, far below the 8-bit weight step).
 */
#define SCT_Q8_BLOCK 32u
#define SCT_Q8_BLOCK_BYTES 34u
size_t sct_q8_row_bytes(size_t columns);
void sct_q8_quantize_row(const float *weights,uint8_t *row,size_t columns);
void sct_q8_expand_row(const uint8_t *row,float *weights,size_t columns);
int sct_matvec_rows_q8(const uint8_t *,const float *,float *,size_t rows,size_t columns);
int sct_matvec_rows_q8_scalar(const uint8_t *,const float *,float *,size_t,size_t);

/* Q4: lossy 4-bit weights. A row is ceil(columns/32) blocks of
 * [uint16 scale][16 bytes (32 x 4-bit nibbles, low nibble first)];
 * weight = scale * (nibble - 8), where nibble in 0..15 represents -8..+7.
 * Block size is 18 bytes (4.5 bits/weight). Activations are int16.
 */
#define SCT_Q4_BLOCK 32u
#define SCT_Q4_BLOCK_BYTES 18u
size_t sct_q4_row_bytes(size_t columns);
void sct_q4_quantize_row(const float *weights,uint8_t *row,size_t columns);
void sct_q4_expand_row(const uint8_t *row,float *weights,size_t columns);
int sct_matvec_rows_q4(const uint8_t *,const float *,float *,size_t rows,size_t columns);
int sct_matvec_rows_q4_scalar(const uint8_t *,const float *,float *,size_t,size_t);

/* Dynamic activation sparsity (PowerInfer-style neuron clustering).
 * Clusters are 32 neurons. active_mask has ceil(hidden/32) bytes (1 = active, 0 = skipped).
 */
#define SCT_SPARSE_CLUSTER 32u
size_t sct_sparse_eval_clusters(const float *gate, uint8_t *active_mask, size_t hidden, float threshold);
void sct_swiglu_sparse(const float *gate, const float *up, float *out, const uint8_t *active_mask, size_t n);
int sct_matvec_rows_q4_sparse(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask);
int sct_matvec_rows_q4_sparse_scalar(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask);
int sct_matvec_rows_q4_sparse_stride(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask, size_t cluster_stride);
int sct_matvec_rows_q4_sparse_scalar_stride(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask, size_t cluster_stride);
int sct_matvec_down_q4_sparse(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask);
int sct_matvec_down_q4_sparse_scalar(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask);
int sct_matvec_down_q4_sparse_stride(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask, size_t cluster_stride);
int sct_matvec_down_q4_sparse_scalar_stride(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask, size_t cluster_stride);
int sct_matvec_rows_q8_sparse(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask);
int sct_matvec_rows_q8_sparse_scalar(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask);
int sct_matvec_rows_q8_sparse_stride(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask, size_t cluster_stride);
int sct_matvec_rows_q8_sparse_scalar_stride(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask, size_t cluster_stride);
int sct_matvec_down_q8_sparse(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask);
int sct_matvec_down_q8_sparse_scalar(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask);
int sct_matvec_down_q8_sparse_stride(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask, size_t cluster_stride);
int sct_matvec_down_q8_sparse_scalar_stride(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns, const uint8_t *active_mask, size_t cluster_stride);
int sct_matvec_rows_q4_batch(const uint8_t *w, const float *x, float *y, size_t m, size_t rows, size_t columns, size_t x_stride, size_t y_stride);
int sct_matvec_rows_q8_batch(const uint8_t *w, const float *x, float *y, size_t m, size_t rows, size_t columns, size_t x_stride, size_t y_stride);
int sct_matvec_qkv_q4(const uint8_t *wq, const uint8_t *wk, const uint8_t *wv, const float *x, float *q, float *k, float *v, size_t rows_q, size_t rows_kv, size_t columns);
int sct_ffn_gate_up_swiglu_q4(const uint8_t *w_gate, const uint8_t *w_up, const float *x, float *out, size_t rows, size_t columns);
/* Repacked 4-row interleaved Q4_4x format (72 bytes per 4 rows x 32 cols + standard Q4 tail). */
size_t sct_q4p_matrix_bytes(size_t rows, size_t columns);
void sct_q4_quantize_row_mse(const float *w, uint8_t *row, size_t columns);
void sct_q4p_pack_matrix(const float *wide, uint8_t *out, size_t rows, size_t columns);
int sct_matvec_rows_q4p(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns);
int sct_matvec_rows_q4p_scalar(const uint8_t *w, const float *x, float *y, size_t rows, size_t columns);
int sct_matvec_qkv_q4p(const uint8_t *wq, const uint8_t *wk, const uint8_t *wv, const float *x, float *q, float *k, float *v, size_t rows_q, size_t rows_kv, size_t columns);
int sct_ffn_gate_up_swiglu_q4p(const uint8_t *w_gate, const uint8_t *w_up, const float *x, float *out, size_t rows, size_t columns);
int sct_matvec_rows_q4p_batch(const uint8_t *w, const float *x, float *y, size_t m, size_t rows, size_t columns, size_t x_stride, size_t y_stride);
int sct_q4_qkv_batch(const uint8_t *wq, const uint8_t *wk, const uint8_t *wv, const float *x, float *q, float *k, float *v, size_t m, size_t rows_q, size_t rows_kv, size_t columns, size_t x_stride);
int sct_q4_gate_up_swiglu_batch(const uint8_t *w_gate, const uint8_t *w_up, const float *x, float *out, size_t m, size_t rows, size_t columns, size_t x_stride);
int sct_matvec_rows_q4_batch_i8(const uint8_t *w, const float *x, float *y, size_t m, size_t rows, size_t columns, size_t x_stride, size_t y_stride);


/* Softmax is stable (subtract max), RMSNorm uses double sum-of-squares:
 * y[i] = x[i] * weight[i] / sqrt(mean(x*x) + epsilon).
 * Inputs/weights must be finite. epsilon must be finite and positive.
 * In-place output is allowed; partial overlap is rejected. Empty n is a no-op.
 * Validation completes before output changes. RMSNorm rejects unrepresentable
 * results with ERANGE; Softmax uses a vector exp approximation on [-80,0]
 * and libm expf in the far tail. Error contracts are in README.md.
 */
int sct_softmax(const float *, float *, size_t);
int sct_softmax_scalar(const float *, float *, size_t);
int sct_softmax_avx2(const float *, float *, size_t);
int sct_softmax_neon(const float *, float *, size_t);
int sct_rmsnorm(const float *, const float *, float *, size_t, float epsilon);
int sct_rmsnorm_scalar(const float *, const float *, float *, size_t, float);
int sct_rmsnorm_avx2(const float *, const float *, float *, size_t, float);
int sct_rmsnorm_neon(const float *, const float *, float *, size_t, float);

/* SwiGLU: out[i] = (gate[i] / (1 + exp(-gate[i]))) * up[i]. */
int sct_swiglu(const float *gate, const float *up, float *out, size_t n);
int sct_swiglu_scalar(const float *gate, const float *up, float *out, size_t n);
int sct_swiglu_avx2(const float *gate, const float *up, float *out, size_t n);
int sct_swiglu_neon(const float *gate, const float *up, float *out, size_t n);

/* Sets the number of OpenMP worker threads used by parallel kernels (0 = default/auto). */
void sct_set_num_threads(int threads);
#endif
