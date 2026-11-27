# Desempenho e escolha de configuração

## Versão 0.15.0: formato `Q4P`, threads dinâmicas, KV cache persistente e GPU

### CPU (`subspace`, Debian 13, Intel Core i7-9700, 8 núcleos, `Qwen3-4B-Instruct` em `Q4P` — 2,3 GiB)

Medições de 26/11/2026 com prompt de 84 tokens e geração de 48 tokens:

| Threads | Prefill (84 tok) | Prefill tok/s | Decode (48 tok) | Decode tok/s | Total |
|---:|---:|---:|---:|---:|---:|
| 4 | 2,141 s | 39,2 tok/s | 3,424 s | 14,02 tok/s | 5,565 s |
| **6 (padrão `auto`)** | **1,457 s** | **57,7 tok/s** | **3,179 s** | **15,10 tok/s** | **4,636 s** |
| 7 | 1,303 s | 64,5 tok/s | 3,342 s | 14,36 tok/s | 4,645 s |
| 8 (sem reserva) | 5,174 s | 16,2 tok/s | 8,102 s | 5,92 tok/s | 13,276 s |

Em servidores com ≥ 6 núcleos, `sct_auto_threads()` reserva automaticamente 2 núcleos para o sistema operacional e proxy HTTP, evitando contenção nas barreiras OpenMP e mantendo o ponto ótimo de prefill e decode.

### Comparação direta no `subspace` (511 e 1.990 tokens de prompt + 64 gerados)

Medições de 26/11/2026 com `schipia 0.15.0` (`auto` = 6 threads em Intel Core i7-9700, modelos `Q4P`/`Q4`) frente às medições anteriores da `0.12.0` e do `llama.cpp` na mesma máquina:

| Perfil e entrada | SchipIA `0.12.0` (Q8, 4t) | `llama.cpp` (Q8, 4t) | **SchipIA `0.15.0` (`Q4P`/`Q4`, `auto=6t`)** | Prefill / Decode (`0.15.0`) | Pico RAM `0.15.0` (vs `llama.cpp`) |
|---|---:|---:|---:|---:|---:|
| Coder 1,5B, ~512 tokens (511t) | 8,69 s | 8,43 s | **4,12 s** (**2,05× mais rápido**) | 157,7 / 73,6 tok/s | **1.077 MiB** (vs 1.890 MiB, -43%) |
| Qwen3-4B, ~512 tokens (511t) | 24,25 s | 23,51 s | **15,95 s** (**1,47× mais rápido**) | 48,9 / 11,7 tok/s | **2.514 MiB** (vs 5.343 MiB, -53%) |
| Qwen3-4B, ~2.000 tokens (1.990t) | 108,50 s | 72,61 s | **62,57 s** (**1,16× mais rápido**) | 35,9 / 8,9 tok/s | **2.791 MiB** (vs 5.425 MiB, -49%) |

### Produção HTTP (`https://schipia.schip.com.br` com cache KV persistente em `libschiptree`)

| Métrica | Antes (`0.14.0` Q8) | Depois (`0.15.0` Q4P + KV cache) |
|---|---:|---:|
| Tempo total de resposta completa (HTTP) | 60,50 s | **16,43 s** (**3,7× mais rápido**) |
| Prefill com prefixo em cache (234 tokens) | ~5,8 s | **0,67 s** |
| Tamanho do modelo `qwen3` em disco/RAM | ~4,3 GiB (Q8) | **2,3 GiB (`Q4P`)** |

### GPU (`Q4P` 2048×2048, lote `m=4`, validado contra referência escalar de CPU)

| Backend / GPU | Erro máximo vs CPU | Tempo médio / matmul |
|---|---:|---:|
| CUDA (`SCHIPIA_GPU=cuda`, NVIDIA GTX 1650) | `8.101e-06` | `2,297 ms` |
| Vulkan (`SCHIPIA_GPU=vulkan`, NVIDIA GTX 1650) | `7.987e-06` | `1,408 ms` |
| Vulkan iGPU (`SCHIPIA_GPU=vulkan`, AMD Radeon Vega) | `8.076e-06` | **`0,625 ms`** |

## Histórico: atualização 0.12.0 e contextos maiores

Atualize pelo APT e mantenha os modelos existentes. O processamento dos prompts
Qwen foi otimizado automaticamente nas CPUs AVX2/FMA compatíveis. As demais
CPUs continuam usando o caminho compatível. Não é necessário mudar a API.

Medições de 16/11/2026 no subspace: Debian 13, Core i7-9700, quatro núcleos,
três repetições por caso. Comparação com a 0.11.0 do laboratório, com os mesmos
modelos e pesos. O tempo de prompt exclui a geração; os ciclos incluem a execução
completa. Foram gerados 64 tokens; embedding não gera texto.

| Perfil e entrada | Redução do tempo de prompt | Redução dos ciclos totais | Aumento do pico de RAM |
|---|---:|---:|---:|
| Embedding 0,6B Q8, 512 tokens | 1,7% | 2,0% | 6,6 MiB |
| Coder 1,5B Q8, 512 tokens | 10,5% | 6,5% | 12,6 MiB |
| Qwen3-4B Q8, 512 tokens | 13,0% | 9,0% | 1,2 MiB |
| Coder 7B Q4, 512 tokens | 22,7% | 18,5% | 11,7 MiB |
| Qwen3-4B Q8, 2.000 tokens | 8,7% | 8,0% | 0,7 MiB |

Contra llama.cpp, com os mesmos pesos e quatro núcleos:

| Perfil e entrada | SchipIA: total | llama.cpp: total | SchipIA: pico RAM | llama.cpp: pico RAM |
|---|---:|---:|---:|---:|
| Coder 1,5B Q8, 512 tokens | 8,69 s | 8,43 s | 1.705 MiB | 1.890 MiB |
| Qwen3-4B Q8, 512 tokens | 24,25 s | 23,51 s | 4.381 MiB | 5.343 MiB |
| Coder 7B Q4, 512 tokens | 32,35 s | 28,20 s | 3.938 MiB | 8.504 MiB |
| Qwen3-4B Q8, 2.000 tokens | 108,50 s | 72,61 s | 4.813 MiB | 5.425 MiB |

O llama.cpp foi mais rápido nos contextos longos medidos; a SchipIA utilizou
menos RAM. Os números incluem abertura e geração e não são latências de HTTP.
O pico de RAM do processo não equivale à memória estável de uma central.

As 15 comparações de saída da SchipIA anterior com a nova coincidiram.
Na comparação de qualidade com llama.cpp, ambos obtiveram 5/5 no helpdesk,
4/5 no Coder 1,5B, 5/5 no Coder 7B e 3/3 na busca por embeddings.
São amostras pequenas; não garantem a correção de respostas futuras.

Para diálogos maiores, comece com timeout de 300 segundos no cliente e no proxy
e ajuste à carga. Envie apenas histórico e documentos necessários. Os resultados
foram obtidos no laboratório separado, acessível conforme [Modelos](MODELOS.md);
a central pública também usa Qwen na versão 0.12.0, com base e estado próprios.

## Helpdesk e programação na versão 0.10.0

Resultados do laboratório no subspace, Core i7-9700, Debian 13 / amd64,
quatro threads de CPU. Qwen3-4B-Instruct-2507 em Q8 para diálogo e
Qwen2.5-Coder-7B-Instruct em Q4 para programação. Os testes usaram o pacote
0.10.0 instalado. A nova geração permanece no laboratório.

| Medida | 0.9.0 | 0.10.0 |
|---|---|---|
| Critérios factuais atendidos, mesmas cinco perguntas | 3/5 | 5/5 |
| Tempo médio dessas perguntas | 1,234 s | 28,797 s |

A versão nova elabora respostas com contexto e tem maior custo de CPU.
Em uma consulta para listar backups, os ciclos acumulados das quatro threads
foram 24.249.658.026 na 0.9.0 e 244.166.864.985 na 0.10.0. Essa medida descreve
uma consulta completa; não é uma taxa geral de ciclos por token.

Na repetição de uma pergunta com resposta idêntica, o tempo caiu de 13,813 s
para 6,135 s com reaproveitamento do contexto. Houve aprovação em 33/33 casos
de recuperação e recusa e nos testes de autorização, isolamento e persistência.

O Coder-7B passou em cinco exercícios C, com o código compilado e executado em
contêineres isolados. As gerações levaram de 7,909 a 29,257 segundos. É uma
amostra pequena: não representa uma garantia de correção para outros programas.
O Coder-1.5B é uma alternativa menor, com mais falhas na avaliação realizada.

Configure cliente e proxy para aguardar a geração; 180 segundos é um ponto
de partida no laboratório. Veja [Modelos](MODELOS.md) e [API](API.md).

## Geração local com o modelo legado Tucano

Medições de 12/11/2026, Core i7-9700, Debian amd64, Tucano-160M, oito histórias
em português e 806 previsões. Valores abaixo são medianas de três execuções com
os mesmos textos e limites. Eles descrevem essa máquina e essa amostra; não são
uma promessa para qualquer equipamento ou história.

## Formato, velocidade e memória

Taxa de inferência medida pelo avaliador, com dados já acessíveis em memória:

| Modelo | 1 thread, tokens/s | 4 threads, tokens/s | Memória residente, 4 threads | Perplexidade |
|---|---:|---:|---:|---:|
| BF16 fornecido | 65,05 | 108,99 | 314,43 MiB | 27,547 |
| Q8 | 109,76 | 198,62 | 186,71 MiB | 27,556 |
| Q4 | 127,75 | 341,08 | 117,28 MiB | 37,155 |

Menor perplexidade significa melhor previsão no mesmo texto. Q8 apresentou um
bom compromisso entre memória, taxa e qualidade nesta amostra. Q4 foi cerca de
3,13 vezes mais rápido que BF16 com 4 threads, mas perdeu qualidade de previsão.
Confira o conteúdo gerado para a sua tarefa antes de escolher apenas pela taxa.

## Geração de uma história no terminal

Também foram medidas chamadas completas de `engine_run`: prefixo fixo em
português, 128 tokens novos, contexto novo, 4 threads, três repetições. Esses
tempos incluem abertura, processamento do prompt, geração e salvamento.

| Modelo | Sem sorteio, tokens/s | Com sorteio e penalidade, tokens/s |
|---|---:|---:|
| BF16 | 94,50 | 76,86 |
| Q8 | 164,64 | 119,00 |
| Q4 | 274,15 | 161,69 |

Configuração com sorteio: `--temp 0.7 --top-p 0.9 --top-k 40
--repeat-penalty 1.15 --seed 42`. As três repetições de cada configuração
produziram o mesmo texto; isso não avalia a qualidade literária da história.
O custo das opções de geração explica por que esses valores são menores que
a taxa do avaliador.

## O que mudou na atualização

A comparação do executável 0.8.0 instalado com o 0.8.1, nos mesmos arquivos,
ficou praticamente empatada: com 4 threads, BF16 passou de 109,01 para 108,99;
Q8, de 198,65 para 198,62; Q4, de 341,57 para 341,08 tokens/s.
Essas diferenças inferiores a 0,2% não demonstram ganho ou perda relevante.
A atualização disponibiliza os novos comandos e mantém o desempenho denso.

## Árvore, alinhamento e esparsidade

No mesmo teste, com Q4 e 4 threads:

| Configuração | Tokens/s | Perplexidade |
|---|---:|---:|
| Arquivo comum, sem esparsidade | 341,90 | 37,155 |
| Bundle em árvore, sem esparsidade | 340,82 | 37,155 |
| Bundle alinhado, sem esparsidade | 323,07 | 37,157 |
| Arquivo comum, `--sparse 0.3` | 332,05 | 41,656 |
| Arquivo comum, `--sparse 0.5` | 331,88 | 315,446 |

O bundle em árvore não trouxe ganho de velocidade nesse cenário. O alinhamento
foi mais lento e apresentou pequena diferença numérica. Esta medição não testa
paginação de modelos maiores que a memória nem desempenho de SSD a frio.
Esparsidade positiva piorou a qualidade e não compensou em tempo. Para o modelo
fornecido e essa utilização, mantenha **`--sparse 0`**.

O avaliador mede previsão de tokens. Sorteio, penalidade de repetição, abertura
do processo e gravação do contexto acrescentam tempo à geração de histórias;
o servidor HTTP também tem seu próprio custo. A taxa do avaliador não deve ser
interpretada como garantia de taxa de uma requisição HTTP ou da CLI com sorteio.

Para reproduzir uma comparação de uso, veja [Ferramentas](FERRAMENTAS.md).


## Otimizações CPU e benchmarks de kernels (13/11/2026)

Principais otimizações implementadas nos binários atuais:
- **Micro-tiling 4-row (`q4_4rows`, `q8_4rows`)** com `_mm_prefetch` (`_MM_HINT_T0`) para maximizar reuso de ativações Q8 em registradores AVX2/FMA (e suporte a ARM64 NEON).
- **Fusão de operadores Q4**: `sct_matvec_qkv_q4` (Q/K/V em uma passada) e `sct_ffn_gate_up_swiglu_q4` (Gate+Up+SiLU em registradores com `exp8` vetorizado bit-exact).
- **Prefill em lote (`sct_forward_batch`)**: multiplicação multi-token (`sct_rows_q4_batch_avx2_raw`) durante o processamento do prompt.
- **Amostragem Top-K com min-heap `O(V log K)`**, verificação `finite_vector` em AVX2 inteiro e eliminação de `memcpy` em blocos KV completos na atenção.

Medições locais (`./bin/bench_kernels` e `./bin/bench_transformer`, `avx2/fma`, gcc `14.2.0`):

| Benchmark | Resultado | Speedup vs escalar/auto-vec |
|---|---:|---:|
| `gemm M=64 N=64 K=64` | `87.96 GF/s` | `1.94x` vs auto-vec |
| `gemm M=256 N=256 K=256` | `75.72 GF/s` | `2.49x` vs auto-vec |
| `gemm M=512 N=512 K=512` | `61.48 GF/s` | `2.15x` vs auto-vec |
| `gemm M=1 N=1024 K=1024` | `47.26 GF/s` | `5.47x` vs auto-vec |
| `softmax n=4096` | `1.239 ns/el` | `5.57x` vs escalar |
| `rmsnorm n=4096` | `1.793 ns/el` | `1.74x` vs escalar |
| `swiglu n=4096` | `1.723 ns/el` | `4.31x` vs escalar |
| `bench_transformer` (`dim=64, hidden=128, heads=4, layers=2`) | `38539.2 tokens/s` | `bit_exact=1` |
