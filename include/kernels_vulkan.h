#ifndef SCHIPIA_KERNELS_VULKAN_H
#define SCHIPIA_KERNELS_VULKAN_H
#include <stddef.h>
#include <stdint.h>

int sct_vulkan_enabled(void);
int sct_vulkan_matvec_q4p(const uint8_t *w, const float *x, float *y,
                          size_t rows, size_t columns);
int sct_vulkan_matvec_q4p_batch(const uint8_t *w, const float *x, float *y,
                                size_t m, size_t rows, size_t columns,
                                size_t x_stride, size_t y_stride);
#endif
