# Desempenho e escolha de configuração

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
