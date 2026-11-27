# Exemplos em C — SchipIA 0.15.0

Estes programas em C demonstram todos os recursos da biblioteca `libschipia.so` e compilam diretamente contra o SDK entregue neste repositório (sem precisar dos fontes internos do motor):

| Arquivo | O que testa e demonstra |
|---|---|
| `01_kernels_q4p.c` | Empacotamento `Q4P`, matvec single/batch, `RMSNorm`, `Softmax`, `SwiGLU`, detecção de backend CPU e threads dinâmicas (`sct_auto_threads`). |
| `02_gpu_backends.c` | Backends GPU zero-dependência (`SCHIPIA_GPU=cuda` via `libcuda.so.1` e `SCHIPIA_GPU=vulkan` via `libvulkan.so.1`) com validação numérica contra a referência escalar de CPU. |
| `03_memory_and_engine.c` | Memória factual persistente em disco (`libschiptree`) e alocação alinhada (`sct_buffer_alloc`). |
| `04_moe_and_gguf.c` | Pipeline completo: gera um mini-modelo GGUF v3 `qwen2moe` (experts 3D + *shared expert*), importa direto para `Q4P` alinhado a 4096 B/expert (`qw_import_gguf`), ativa streaming `mmap`, LFRU persistente (`qw_enable_moe_heat`) e cache KV (`qw_enable_kv_cache`), validando `qw_forward` vs `qw_prefill_cached`. |
| `05_qwen_generate.c` | Inferência completa com qualquer modelo real Qwen2/Qwen3/MoE (`.qw`/`.qwt`) e vocabulário. |

## Compilar e executar

```sh
make         # compila todos os exemplos em build/examples/
make check   # compila e executa os testes dos exemplos
```

Para testar aceleração de GPU no exemplo 02:

```sh
SCHIPIA_GPU=cuda ./build/examples/02_gpu_backends
SCHIPIA_GPU=vulkan ./build/examples/02_gpu_backends
```
