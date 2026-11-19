# Memória do Projeto — SchipIA

Registro técnico consolidado da arquitetura, recursos implementados, invariantes e resultados verificados em a árvore privada de desenvolvimento.

## Arquitetura e Componentes
- **Kernels CPU (`src/kernels.c`, `src/kernels_avx2.c`, `src/kernels_neon.c`, `src/kernels_scalar.c`)**:
  - Dispatch automático em runtime (`AVX2/FMA` em x86_64, `NEON` em ARM64, `scalar` como referência).
  - Micro-tiling de 4 linhas (`q4_4rows`, `q8_4rows`) com `_mm_prefetch` (`_MM_HINT_T0`) e redução `hsum_avx`.
  - Kernels fundidos Q4: `sct_matvec_qkv_q4` (`sct_q4_qkv_avx2_raw`) e `sct_ffn_gate_up_swiglu_q4` (`sct_q4_gate_up_swiglu_avx2_raw` com `exp8` vetorizado bit-exact via `sct_swiglu_avx2_raw`).
  - Prefill em lote multi-token: `sct_rows_q4_batch_avx2_raw` e `sct_rows_q8_batch_avx2_raw`.
  - Execução esparsa e cluster-major: `sct_rows_q{4,8}_sparse_avx2_raw`, `sct_down_q{4,8}_cluster_avx2_raw` (`SCT_SPARSE_CLUSTER = 32`).
- **Modelo e Empacotamento (`src/model.c`)**:
  - Formatos nativos: `F32`, `BF16`, `Q8_0` (blocos de 32), `Q4_0` (blocos de 32).
  - Suporte arquitetural: GQA (`kv_heads != heads`), `rope_theta`, `rms_eps`, até `SCT_MAX_LAYERS = 48`, `dim <= 8192`, `hidden <= 32768`, `vocab <= 262144`, `max_context <= 131072`.
  - Bundle `libschiptree` (`--tree`): índice B-tree por cluster e deduplicação content-addressed de clusters idênticos em `sct_model_pack_tree_aligned`.
  - Alinhamento de página (`--page-align`, `SCT_MODEL_PAGE_ALIGNED`): clusters alinhados em 4096 bytes e `SCT_DOWN` em layout cluster-major.
- **KV Cache e Prefix Caching (`src/kv_cache.c`)**:
  - Cache persistente mapeado em `mmap` com blocos de `SCT_KV_BLOCK_TOKENS = 8`.
  - Índice `Tschiptree`: `kind=1` (metadados de sequência), `kind=2` (blocos KV da sequência), `kind=3` (índice de prefix caching por hash de 128 bits de bloco de 8 tokens).
- **Transformer e Engine (`src/transformer.c`, `src/engine.c`)**:
  - `sct_forward` e `sct_forward_batch` (usado por `sct_engine_prefill`).
  - Verificação `finite_vector` vetorizada em AVX2 inteiro (imune a `-ffast-math`).
  - Atenção sem `memcpy` redundante em blocos KV completos (`block < last`).
  - Prefetch coalescido condicional `madvise(MADV_WILLNEED)` apenas quando `SCT_INFER_OUT_OF_CORE` está ativo.
  - Amostragem Top-K com min-heap `O(V log K)` + `top_p`, `temperature`, `repeat_penalty`, `seed`.
  - `sct_engine_new_sequence` para alocação automática de IDs de sequência em servidores persistentes.
- **Importadores e Ferramentas (`tools/`)**:
  - `schipia-import-gguf` (`tools/import_gguf.c`): importa GGUF v2/v3 (`F32`, `F16`, `BF16`, `Q8_0`, `Q4_0`, `Q4_K`) para `.sct_model` (Q4) + `.sct_tokens` (BPE).
  - `schipia-import-tucano`, `schipia-import-llama`, `schipia-pack`, `schipia-tokenize`, `convert_vocab`, `schipia-context`, `schipia-eval`, `engine_run`, `schipia-server`.
  - `engine_run`: suporte a `--out-of-core`, `--sparse`, `--chat chatml|llama3`, `--system`.
  - `schipia-server`: engine persistente em memória (sem fork/exec por requisição), `GET /health`, `POST /v1/completions` e `POST /v1/chat/completions` com SSE streaming.
- **SDK Público (`include/schipia.h`, `build/libschipia.so`)**:
  - API C11 completa para embedding em aplicações externas.

## Invariantes Críticas
- `test_transformer` exige igualdade **100% bit-exact** entre caminho denso (`c_dense`), esparso com threshold 0 (`c_sparse`) e bundle em árvore alinhado (`c_pg`). Qualquer otimização SIMD nos kernels Q4 ou SwiGLU deve preservar exatamente a mesma ordem de redução e aproximação `exp8` de `sct_swiglu_avx2_raw`.
- `-ffast-math` é usado em `src/*.c`; verificações de NaN/Inf devem usar bitmask inteiro (`0x7f800000`), nunca `isfinite()`.

## Última Verificação (13/11/2026)
- `make -j$(nproc)` com `-Wall -Wextra -Werror`: 0 erros/warnings.
- `bash tests/run_checks.sh`: `PASS: SchipIA full verification`.
- `bash tests/run_checks.sh --valgrind`: `PASS: Valgrind all units/children and CLI suites, no errors/leaks`.
- `./bin/bench_kernels` (`avx2/fma`, gcc `14.2.0`): `gemm 64=87.96 GF/s`, `gemm 256=75.72 GF/s`, `gemm 512=61.48 GF/s`, `gemm 1x1024x1024=47.26 GF/s`, `softmax=1.239 ns/el`, `rmsnorm=1.793 ns/el`, `swiglu=1.723 ns/el`.
- `./bin/bench_transformer` (`dim=64 hidden=128 heads=4 layers=2 vocab=128 prefix=128`): `forward_tokens_s=38539.2`, `bit_exact=1`.
