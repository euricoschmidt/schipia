#ifndef SCHIPIA_KERNELS_CUDA_H
#define SCHIPIA_KERNELS_CUDA_H
#include <stddef.h>
#include <stdint.h>

int sct_cuda_enabled(void);
int sct_cuda_matvec_q4(const uint8_t *w, const float *x, float *y,
                       size_t rows, size_t columns);
int sct_cuda_matvec_q4_batch(const uint8_t *w, const float *x, float *y,
                             size_t m, size_t rows, size_t columns,
                             size_t x_stride, size_t y_stride);
int sct_cuda_matvec_q4p(const uint8_t *w, const float *x, float *y,
                        size_t rows, size_t columns);
int sct_cuda_matvec_q4p_batch(const uint8_t *w, const float *x, float *y,
                              size_t m, size_t rows, size_t columns,
                              size_t x_stride, size_t y_stride);
#endif
