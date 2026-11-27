/*
 * SchipIA C Public SDK Header
 * High-performance inference engine for local LLMs powered by libschiptree.
 */
#ifndef SCHIPIA_H
#define SCHIPIA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "kernels.h"
#include "tokenizer.h"
#include "transformer.h"
#include "engine.h"
#include "memory_store.h"
#include "helpdesk.h"

#define SCHIPIA_VERSION_STRING "0.15.0"
#define SCHIPIA_VERSION_MAJOR  0
#define SCHIPIA_VERSION_MINOR  15
#define SCHIPIA_VERSION_PATCH  0

#ifdef __cplusplus
}
#endif

#endif /* SCHIPIA_H */
