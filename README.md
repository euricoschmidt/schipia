# SchipIA — manual do usuário e distribuição binária

Motor de inferência LLM local em C11 com memória persistente em `libschiptree`,
kernels SIMD (AVX2/FMA e ARM64 NEON), quantização BF16/Q8/Q4, importador GGUF,
prefix caching persistente, modo esparso/out-of-core e servidor HTTP OpenAI-compatível.

Este repositório inclui os **executáveis pré-compilados** (`bin/`), **bibliotecas**
(`lib/`) e **headers públicos do SDK C** (`include/`), além da documentação completa
para testar todos os recursos da engine (sem publicação dos arquivos-fonte).

## Comece em 30 segundos

```sh
export LD_LIBRARY_PATH="$PWD/lib:${LD_LIBRARY_PATH:-}"
./bin/engine_run demo /tmp/schipia-demo 42
./bin/bench_kernels
./bin/test_kernels
```

Para modelos reais:
1. [Instale ou execute diretamente com `bin/` e `lib/`](INSTALACAO.md).
2. [Importe um modelo GGUF ou use Tucano/LLaMA](MODELOS.md).
3. [Gere texto, use templates de chat e sessões persistentes](USO.md).
4. [Suba o servidor HTTP (`/v1/completions` e `/v1/chat/completions`)](HTTP.md).

## Todos os recursos

| Guia | Conteúdo |
|---|---|
| [Instalação](INSTALACAO.md) | Execução direta via `bin/`+`lib/`, pacote Debian/APT, permissões e SDK C. |
| [Uso no terminal](USO.md) | Geração, amostragem Top-K/Top-P, `--chat`, `--system`, `--sparse`, `--out-of-core`, sessões e memória associativa. |
| [Modelos](MODELOS.md) | Importador `schipia-import-gguf` (F32/F16/BF16/Q8_0/Q4_0/Q4_K), GQA/RoPE, `schipia-pack` (`--q4`/`--q8`/`--tree`/`--page-align`). |
| [Servidor HTTP](HTTP.md) | Motor persistente, `GET /health`, `POST /v1/completions`, `POST /v1/chat/completions` e streaming SSE. |
| [Ferramentas e SDK](FERRAMENTAS.md) | Catálogo completo dos executáveis em `bin/`, benchmarks (`bench_*`), testes (`test_*`) e SDK C (`libschipia.so`). |
| [Desempenho](DESEMPENHO.md) | Medições de kernels/transformer, otimizações AVX2/FMA, memória e escolha de configuração. |
| [Solução de problemas](SOLUCAO_DE_PROBLEMAS.md) | Diagnóstico de `LD_LIBRARY_PATH`, contexto, permissões, geração e servidor. |
| [Licença e componentes de terceiros](LICENCIAMENTO.md) | Termos de avaliação, produção, OEM e avisos preservados. |

Distribuição e dependências: <https://deb.schip.com.br>.
Contato comercial e suporte: **euricosalvesschmidt@gmail.com**.

Copyright (c) 1988 Eurico Schmidt. Todos os direitos reservados.
Leia o [contrato de licença](LICENSE) antes de utilizar o produto.
