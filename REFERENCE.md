# SchipIA 0.10.0 — central de ajuda e inferência em C11

Novos perfis opcionais: Qwen3-4B para diálogo, Qwen3-Embedding para busca
semântica e Qwen2.5-Coder-7B para programação, com opção rápida de 1,5B.
Consulte o [guia de modelos](docs/references/9e9d0fb1-MODELOS.md) para instalação, ativação e API.
Os pesos são instalados separadamente do pacote de execução. O laboratório
permanece no subspace.

A central de ajuda reúne documentação de 18 aplicativos e atende pela API
autenticada em `https://schipia.schip.com.br`. Ela respeita aplicativo, versão,
tela, perfil e organização, apresenta fontes e retém avaliações e contribuições.
Informações novas passam por aprovação antes de entrar nas respostas.
Veja [uso da central](docs/references/2ca9f0d1-USO.md) e [integração e administração](docs/references/d667c850-INTEGRACAO.md).

Motor C11 de inferência local, memória persistente e tokenização usando libschiptree.
As cinco fases do protótipo estão implementadas, incluindo `bin/engine_run`, `bin/schipia-server`,
`bin/schipia-import-gguf` e a biblioteca compartilhada `build/libschipia.so`.
Inclui pesos reais Tucano-160M, importador GGUF v2/v3, tokenizador BPE exato e índice KV híbrido.
Os conversores JSON usam a **libschipjson**.

Recursos recentes documentados em [MEMORY.md](MEMORY.md):
- Kernels AVX2/FMA e ARM64 NEON com micro-tiling de 4 linhas (`q4_4rows`/`q8_4rows`), fusão QKV e Gate+Up+SwiGLU, e prefill em lote (`sct_forward_batch`).
- Suporte a GQA (`kv_heads`), `rope_theta`, `rms_eps` e importação direta de modelos GGUF (`F32`/`F16`/`BF16`/`Q8_0`/`Q4_0`/`Q4_K` via `bin/schipia-import-gguf`).
- Prefix caching persistente em `libschiptree` (blocos de 8 tokens KV) e amostragem Top-K com min-heap.
- Execução esparsa e out-of-core (`--sparse`, `--tree`, `--page-align`, `--out-of-core`) com deduplicação de clusters.
- Templates de chat (`--chat chatml|llama3`, `--system`) em `engine_run` e endpoint `POST /v1/chat/completions` com engine persistente em `schipia-server`.

Licenciado conforme [LICENSE](LICENSE), nos mesmos termos da libschiptree.
Os avisos MIT de llama2.c e Apache-2.0 do modelo Tucano permanecem aplicáveis
aos respectivos componentes, com seus textos preservados nos diretórios originais.
Documentação pública de utilização: [manual do usuário](docs/references/11b791d0-README.md).

## Modelo treinado em português

O `.deb` inclui **TucanoBR/Tucano-160m** (162.417.408 parâmetros), nativamente
pré-treinado em português, distribuído em BF16 nativo, sem alteração dos pesos. A tarefa
escolhida é continuar histórias curtas. É um modelo causal geral; não recebeu
ajuste para histórias infantis nem instruções de chat. Exemplos podem repetir,
errar ou perder coerência. A memória factual permanece separada dos pesos.

No `root@subspace`, depois de instalar o pacote:

```sh
model=/usr/share/schipia/models/tucano160m
engine_run generate "$model/model.sct_model" "$model/vocab.sct_tokens" /var/tmp/minha-historia 1 \
  --new --prompt 'Era uma vez uma menina chamada Ana que encontrou uma chave no jardim.' --tokens 64 \
  --temp 0.7 --top-p 0.9 --repeat-penalty 1.15
engine_run generate "$model/model.sct_model" "$model/vocab.sct_tokens" /var/tmp/minha-historia 1 --tokens 32
schipia-eval "$model/model.sct_model" "$model/vocab.sct_tokens" \
  /usr/share/schipia/fixtures/historias-pt.txt 8 128
```

O modo padrão do modelo treinado mostra texto UTF-8 e escapa controles. Contexto
máximo: **2048 tokens**, somando prompt e continuação. Inferência AVX2 vetorizada
e paralela OpenMP (**108 tok/s** com 4 threads no i7-9700 em BF16, latência mediana de **9,2 ms**; 65 tok/s com 1 thread;
o 0.6.0 em FP32 fazia 54 e 34 tok/s).
O tokenizador de origem não acrescenta BOS/EOS automaticamente; a geração encerra ao emitir BOS/EOS.
Suporta amostragem estocástica com penalidade de repetição para histórias ricas
sem repetição de frases (`--temp`, `--top-p`, `--top-k`, `--repeat-penalty`, `--seed`, `--threads`).
Quando `--temp 0.0` (padrão), opera em modo guloso determinístico bit-a-bit.
Prompts devem ser prefixos de histórias, como “Era uma vez...”, e não ordens.

### Pesos BF16 nativos, sem perdas (formato v3)

O checkpoint original do Tucano é BF16. Expandi-lo para FP32 dobra os bytes lidos
por token sem acrescentar informação, e a inferência token a token é limitada pela
banda de memória (552 MB de pesos FP32 percorridos a cada token). `schipia-pack`
regrava o modelo com matrizes em BF16 `[saída][entrada]` e expande cada peso
exatamente para FP32 dentro do laço FMA (`vpmovzxwd` + deslocamento):

```sh
schipia-pack fp32/model.sct_model bf16/model.sct_model
cp -r fp32/vocab.sct_tokens bf16/
```

A conversão só existe na forma exata: se algum peso tiver os 16 bits baixos
diferentes de zero, falha com `ERANGE` e não grava nada. Normas RMS continuam FP32.
No i7-9700: **65 / 98 / 108 / 110 tok/s** com 1/2/4/8 threads (FP32: 34 / 47 / 54 / 53),
arquivo de 325 MB em vez de 650 MB e RSS de 322 MB em vez de 595 MB. Cada logit é um
produto escalar independente por linha, então o resultado não depende do número de
threads. O modelo empacotado tem identidade própria: caches KV do modelo FP32 não
são reutilizados. O pacote 0.7.0 já distribui o modelo neste formato. No mesmo
checkpoint, o llama.cpp faz 56 / 94 / 110 / 120 tok/s em BF16 e 99 / 166 / 202 / 216
em Q8_0 (com perda). Detalhes e comparação com PyTorch em `tests/RESULTS_PT.md`.

### Q8: pesos de 8 bits, com perda pequena e medida

`schipia-pack --q8` aceita o modelo FP32 ou o BF16 instalado e grava blocos de 32
pesos int8 com uma escala de 16 bits cada (8,5 bits por peso, mesma granularidade do
Q8_0 do llama.cpp). O produto é inteiro: as ativações são arredondadas para int16
uma vez por matriz e multiplicadas com `vpmaddwd`.

```sh
schipia-pack --q8 "$model/model.sct_model" /var/tmp/tucano-q8/model.sct_model
cp -r "$model/vocab.sct_tokens" /var/tmp/tucano-q8/
```

No i7-9700: **110 / 175 / 199 / ~205 tok/s** com 1/2/4/8 threads, arquivo de 173 MB e
RSS de 191 MB. Não é sem perdas: no corpus de regressão o NLL passa de 3,31590 para
3,31622 e o top-1 de 31,76% para 31,64%; a continuação gulosa do prompt de teste é
a mesma nos primeiros 32 tokens. O pacote continua distribuindo o BF16 (exato) como
padrão; o Q8 é uma conversão explícita de menos de um segundo.

### Q4: pesos de 4 bits para superar o llama.cpp Q4_K_M

`schipia-pack --q4` aceita o modelo FP32, BF16 ou Q8 e grava blocos de 32 pesos
quantizados em 4 bits (nibbles no intervalo $[-8, +7]$ com offset $+8$) e escala BF16 de
16 bits por bloco (4,5 bits por peso, arquivo de **88 MB** vs 103 MB do GGUF Q4_K_M).
A descompactação SIMD AVX2 extrai os nibbles em paralelo, estende para int16 e realiza
o dot-product via `vpmaddwd` com apenas uma FMA de escala por bloco.

```sh
schipia-pack --q4 "$model/model.sct_model" /var/tmp/tucano-q4/model.sct_model
cp -r "$model/vocab.sct_tokens" /var/tmp/tucano-q4/
```

No i7-9700 (`root@subspace`):
- **1 thread**: **128,0 tok/s**
- **2 threads**: **234,9 tok/s**
- **4 threads**: **340,6 tok/s** (supera os 318,9 tok/s do `llama.cpp Q4_K_M`)
- **8 threads**: **349,2 tok/s** (supera os 342,6 tok/s do `llama.cpp Q4_K_M`)
- Arquivo: **88 MB** (91.691.008 B), RSS de apenas **117 MB**!

### Empacotamento libschiptree e Esparsidade Dinâmica de Neurônios (PowerInfer-2)

Inspirado na tecnologia do **PowerInfer-2**, o SchipIA implementa particionamento dos neurônios do FFN (SwiGLU) em clusters de 32 neurônios e indexação persistente via `libschiptree`:

1. **Empacotamento em Árvore B+ (`schipia-pack --tree [--page-align]`)**:
   Empacota o modelo em um diretório bundle composto por:
   - `model.sct_model`: cabeçalho do modelo e tensores base;
   - `weights.clusters`: tabela de registros mapeando chaves canônicas de 16 bytes `[camada:2B][tensor:2B][cluster:4B][res:8B]` e offset de arquivo;
   - `weights.index`: índice persistente da `libschiptree` associando cada cluster de neurônios.
   - Com `--page-align`: alinha cada cluster de 32 neurônios do FFN a múltiplos exatos de 4096 bytes (página do SO) com padding interno, garantindo **Zero False Paging** ao paginar pesos diretamente de SSD NVMe.

   ```sh
   mkdir -p /var/tmp/tucano-q4
   schipia-pack --q4 "$model/model.sct_model" /var/tmp/tucano-q4/model.sct_model
   schipia-pack --tree --page-align /var/tmp/tucano-q4/model.sct_model /var/tmp/tucano-q4-tree
   ```

2. **Ativação Esparsa Dinâmica e Prefetch Coalescido no SwiGLU FFN**:
   Avalia dinamicamente os clusters de 32 neurônios da projeção gate ($W_{gate}$). Se a magnitude máxima de ativação no cluster estiver abaixo de `--sparse <limiar>` (ex: 0.1 a 0.3), o cálculo dos 32 neurônios correspondentes nas matrizes $W_{up}$ (linhas) e $W_{down}$ (colunas) é totalmente ignorado via SIMD AVX2.
   - **Prefetch Coalescido (`madvise`)**: Antes de iniciar o produto de $W_{up}$, o motor unifica os clusters ativos contíguos em intervalos e dispara `madvise(MADV_WILLNEED)` em lote para o kernel Linux, enfileirando I/O assíncrono em profundidade de fila (Queue Depth alta) sem serializar page faults.
   - `--sparse 0.0` (padrão): execução densa, resultado 100% bit-a-bit idêntico à referência.
   - `--sparse 0.3`: pula clusters inativos, economizando gigabytes de largura de banda de memória em execuções longas.

   ```sh
   engine_run generate /var/tmp/tucano-q4-tree "$model/vocab.sct_tokens" /var/tmp/ctx 1 \
     --new --prompt 'Era uma vez' --tokens 64 --sparse 0.3
   ```

Servidor HTTP de Streaming SSE nativo em C11 incluído (`bin/schipia-server`):
```sh
schipia-server "$model/model.sct_model" "$model/vocab.sct_tokens" 8080 /tmp/srv-cache 4
curl -N -X POST http://127.0.0.1:8080/v1/completions \
  -H "Content-Type: application/json" \
  -d '{"prompt":"Era uma vez um pássaro azul"}'
```

Importação reproduzível, executada no servidor com os três arquivos originais
da revisão documentada em `models/tucano/PROVENANCE.md`:

```sh
schipia-import-tucano model.safetensors config.json tokenizer.json native-pt
```

O adaptador suporta deliberadamente o perfil auditado Tucano-160M: BPE por
ranking exato de **pares**, normalização de espaços `▁`, tokens especiais e
decoder correspondente. Rejeita outros perfis. Pesos, registros e merges usam
mmap. JSON usa libschipjson; só a configuração e o pequeno cabeçalho safetensors
usam DOM. Não há Python, PyTorch ou runtime C++ na inferência/conversão instalada.
`schipia-import-llama` também importa checkpoints FP32 originais llama2.c com
igual número de cabeças Q/KV e seu tokenizador de merges por score.

Os arquivos de índice do vocabulário precisam ser abertos com escrita pela
libschiptree. O bundle instalado é para uso por root; para outro usuário, copie
o diretório do modelo para uma área própria e ajuste a propriedade. O cache de
geração sempre deve pertencer ao usuário que executa o motor.

## Índice persistente e cache rápido

A libschiptree continua como índice autoritativo no disco. Um hash volátil guarda
somente hashes e offsets uint64 dos registros acessados; os registros são
validados no mmap atual, inclusive depois de crescimento/relocação. Ele começa
vazio na reabertura e cresce até **1 MiB**, com descarte ao atingir o limite.
Falha de alocação volta à árvore. Não há reconstrução de tabela inteira ao abrir.
`--no-hot-cache` permite comparar a mesma inferência usando apenas a árvore.
As métricas mostram consultas reais à árvore, hits e bytes do cache rápido.

## Qualidade e validação

Vinte entradas de referência verificam IDs exatos, incluindo acentos, Unicode
decomposto, emoji, tokens especiais, NUL, tabs e espaços. 832.000 logits e a
continuação gulosa são comparados ao Transformers/Tokenizers original; também
se verifica igualdade bit a bit com cache rápido desativado e reabertura mmap.
O teste de saturação cobre 40.000 sequências e descarte no limite de 1 MiB.
O `.deb` contém o oráculo binário de referência; o teste instalado não precisa
de Python. A referência pode ser regenerada com `tests/reference_tucano.py`
somente no ambiente separado de teste.

`tests/fixtures/historias-pt.txt` contém oito histórias novas para regressão.
Não é um benchmark amplo de literatura. `schipia-eval` calcula NLL, perplexidade,
top-1, latência mediana/p95, RSS e contadores de ciclos/instruções de hardware
quando `perf_event_open` está disponível. Mede ciclos no forward, excluindo
kernel/hipervisor; leitura da árvore faz parte do forward. As medidas são de CPU
real e não estimativas derivadas de FLOPs. Resultados em `tests/RESULTS_PT.md`.

## CLI integrada da Fase 5

```sh
engine_run demo exemplo 123
engine_run generate exemplo/model.sct_model exemplo/vocab.sct_tokens contexto 42 \
  --new --prompt 'Olá mundo' --tokens 8
engine_run inspect exemplo/model.sct_model exemplo/vocab.sct_tokens contexto 42
engine_run generate exemplo/model.sct_model exemplo/vocab.sct_tokens contexto 42 --tokens 8
engine_run interactive exemplo/model.sct_model exemplo/vocab.sct_tokens conversa 7 --new --tokens 8
engine_run memory-put fatos 'cidade:São Paulo' 42
engine_run memory-get fatos 'cidade:São Paulo'
```

`demo` cria um modelo **sintético**, não treinado, e vocabulário de 256 tokens
de um byte cada (IDs 0–255), incluindo NUL. Não produz linguagem útil nem
substitui um checkpoint treinado. O forward continua sendo o Transformer
implementado na Fase 4; a CLI agora liga suas entradas/saídas ao tokenizador.

`generate` faz preflight do prompt e capacidade, forward do prompt e geração
(gulosa por padrão ou estocástica se `--temp > 0`). Empates escolhem o menor ID.
`--prompt-file` mapeia entrada binária; `--tokens 0` apenas ingere o prompt.
`--eos ID` encerra após incluir e persistir o EOS gerado nessa chamada.
Sem prompt, continua dos últimos logits, sem replay.
`--temp FLOAT`: temperatura (padrão 0.0 = argmax guloso, ex: 0.7 para amostragem).
`--top-p FLOAT`: corte de probabilidade acumulada Nucleus (padrão 1.0, ex: 0.9).
`--top-k N`: limita a escolha aos N tokens mais prováveis (padrão 0 = desativado).
`--repeat-penalty FLOAT`: penaliza tokens já presentes no histórico recente (ex: 1.15).
`--seed N`: semente para o gerador de números pseudoaleatórios xorshift64*.
`--sparse FLOAT`: limiar de esparsidade dinâmica para clusters de neurônios no FFN (padrão 0.0 = denso).
Um contexto vazio precisa de prompt para gerar; o perfil legado llama2.c insere BOS.
`--new` cria cache exclusivamente; `--new-sequence` cria outra sequência num
cache existente. `--scalar` usa os kernels escalares da Fase 3.

Saída padrão do modelo sintético é uma linha de IDs; no treinado, é texto.
Métricas seguem em stderr. `--format escaped` preserva UTF-8 válido e escapa controles/bytes inválidos;
`--format raw` emite bytes sem terminador de linha e pode conter NUL, sequências
de terminal e UTF-8 inválido. Use raw para arquivos/pipelines. O checkpoint
é concluído antes da saída: erro de stdout não desfaz os tokens já persistidos.

O modo interativo recebe um prompt por linha, sem o newline final. Comandos
`:state`, `:generate` e `:quit` inspecionam, continuam ou encerram a sessão.
Cada turno bem-sucedido é sincronizado. Erros de preflight não alteram o estado;
erros durante forward mantêm a política dirty/fail-stop da Fase 4. Interrupção
durante escrita pode exigir reconstrução do cache; não há rollback automático.

Memória factual é chave→record ID uint64 na libschiptree, sem aprendizado neural
automático. `memory-put` é insert-only (EEXIST em duplicatas), `memory-get` não
cria um store ausente. Aceita zero e UINT64_MAX; chaves CLI têm 1–256 bytes.

O motor exige vocabulário denso 0..model.vocab-1. Offsets de embedding, quando
associados, devem ser `id*dim*sizeof(float)` relativos ao tensor de embeddings;
os offsets dos tokens usados são verificados antes de seus forwards. Vocabulários
com offsets não associados usam a linha id. O cache fica vinculado ao fingerprint
do vocabulário além da identidade do modelo. Trocar tokens/IDs/offsets impede
retomada, mesmo mantendo a mesma contagem. Ordem de inserção não muda o fingerprint.

Vocabulários v2 guardam fingerprint não criptográfico de 128 bits e ID máximo
no cabeçalho de 128 bytes, sem varredura na abertura do motor. O leitor aceita
v1 e calcula esses metadados uma vez por handle diretamente nos registros mmap.
Caches v2 guardam a vinculação de vocabulário na área reservada do cabeçalho.
Caches v1 continuam legíveis pela API Transformer, mas caches não vazios sem
vinculação não são adotados pela CLI textual. Os arquivos de modelo/vocabulário
devem permanecer imutáveis; não há validação criptográfica de todo o conteúdo.

Verificação completa, sempre no servidor:

```sh
ssh root@subspace /usr/libexec/schipia/run_checks.sh
ssh root@subspace '/usr/libexec/schipia/run_checks.sh --valgrind'
ssh root@subspace '/usr/libexec/schipia/run_checks.sh --benchmarks'
```

`bench_engine` executa a CLI inspect em processos novos e mede inicialização
interna e tempo de fork/exec completo, com mediana de cinco amostras. Tenta
descartar somente os seis arquivos privados de modelo/vocabulário/KV com
fdatasync + POSIX_FADV_DONTNEED; verifica residência com mincore antes de cada
amostra. Só há dados frios verificados quando `cold_verified=5/5` e
`resident_before_max=0`. Executável e bibliotecas compartilhadas não são expulsos
do page cache; não limpa o cache global. A medida abre e inspeciona o contexto,
sem primeiro forward nem carregamento obrigatório de todos os pesos.
É um modelo pequeno, sem limite de
latência prometido. [fadvise](https://man7.org/linux/man-pages/man2/posix_fadvise.2.html)
e [mincore](https://man7.org/linux/man-pages/man2/mincore.2.html) são consultivos/
instantâneos; os números distinguem esta condição de abertura quente.

## Transformer e KV cache da Fase 4

O forward incremental executa embedding → blocos pre-norm de atenção causal
multi-head com RoPE e residual → RMSNorm/SwiGLU/residual → RMSNorm final →
projeção de logits. Todas as projeções usam os kernels da Fase 3. RoPE gira
pares adjacentes dentro de cada head com base 10000; epsilon RMSNorm é 1e-5.
Não há biases, dropout, GQA ou treinamento. O modelo nativo tem tensores float32
row-major e offsets uint64, alinhados em 64 bytes, num arquivo mmap somente de
leitura. Scratch de computação é alocado uma vez com alinhamento 64.

O gerador cria **pesos sintéticos determinísticos para teste**, não um modelo
de linguagem treinado. Não importa checkpoints HuggingFace/GGUF nem promete
geração de texto útil. A CLI da Fase 5 integra o tokenizador e esses IDs.

```sh
schipia-context model teste.sct_model 123
schipia-context new teste.sct_model contexto.sct_kv 42
schipia-context add teste.sct_model contexto.sct_kv 42 0 1 2 3 4 5 6 7 8
schipia-context state teste.sct_model contexto.sct_kv 42
schipia-context add teste.sct_model contexto.sct_kv 42 9
schipia-context sequence teste.sct_model contexto.sct_kv 7
schipia-context add teste.sct_model contexto.sct_kv 7 3 4
```

Cada comando abre um processo separado. `state` informa a posição e os últimos
logits sem forward: `tokens=9 forward_steps_this_open=0`. O próximo `add`
processa só o token novo, lendo K/V anteriores. A atenção continua O(contexto);
a retomada dispensa reprocessar embeddings/projeções dos tokens anteriores.

O diretório `.sct_kv` contém `state.bin` e `blocks.index`. Um cabeçalho de
4096 bytes guarda configuração, identidade do modelo, posição de alocação
e marcador dirty. Registros alinhados em 64 bytes têm chave de 24 bytes:
kind/sequence/block em uint64 big-endian. A libschiptree guarda o offset
relativo do registro e recupera sua chave real pelo callback sobre o mmap.
Descritores por sequência persistem contagem e últimos logits. Blocos de
oito tokens persistem IDs e K/V de todas as camadas; K fica transposta por
head `[head_dim,8]` e V em `[8,head_dim]`, permitindo atenção via GEMM em blocos.
Não há ponteiros persistidos. Arquivos crescem por duplicação/remapeamento.

Há um writer exclusivo por diretório via flock e um handle por thread. O modelo
deve permanecer aberto e imutável enquanto usado pelo contexto. Os formatos
são nativos e exigem ABI compatível. A identidade do modelo é um fingerprint
não criptográfico calculado na criação sobre configuração, seed e pesos;
a abertura confere layout e identidade declarada, sem varrer todos os pesos.
Não modifique os arquivos do modelo externamente: não há verificação de
integridade completa dos tensores na abertura.

Antes da primeira mutação após checkpoint, dirty=1 é sincronizado em disco.
`sct_context_sync` sincroniza dados e índice antes de persistir dirty=0.
O fechamento normal faz checkpoint; use sync explicitamente para conferir erros.
`sct_context_abort` libera recursos sem limpar dirty. Escrita interrompida,
abortada ou erro numérico/I/O durante forward invalida a sessão; uma reabertura
dirty falha com EUCLEAN. Não há rollback/WAL/recuperação automática: reconstrua
o cache a partir do prompt. Não há garantia completa de durabilidade contra
perda de energia nem fsync do diretório pai. Não sobrescreve diretórios existentes.

Configurações: dimensão 1–1024, hidden 1–8192, até 16 camadas, vocabulário
até 65536 e contexto até 65536; todas positivas, dim divisível por heads e
head_dim par. O utilitário gera dim=32, hidden=64, heads=4, layers=2,
vocab=64, contexto=256, com IDs 0–63. Capacidade e IDs são verificados antes
de alterar o cache. `SCT_INFER_SCALAR` permite testar o mesmo grafo sem SIMD.

O teste numérico usa uma implementação independente em double, sem kernels
de produção, contração FMA ou fast-math. O teste de exec compara a retomada
de um prefixo de nove tokens com vinte tokens contínuos, exigindo logits
bit a bit iguais e exatamente onze novos forwards no processo retomado.
O benchmark mede um modelo sintético pequeno, checkpoint e reabertura quente;
não representa desempenho de um LLM treinado nem cold start.

```sh
ssh root@subspace /usr/libexec/schipia/test_transformer
ssh root@subspace /usr/libexec/schipia/test_context.sh
ssh root@subspace /usr/libexec/schipia/bench_transformer
ssh root@subspace 'SCT_VALGRIND=1 /usr/libexec/schipia/test_context.sh'
```

Referências: [RoPE](https://arxiv.org/abs/2104.09864),
[RMSNorm](https://arxiv.org/abs/1910.07467),
[SwiGLU](https://arxiv.org/abs/2002.05202).

## Kernels de computação da Fase 3

`include/kernels.h` oferece GEMM float32 (`C += A×B`, matrizes contíguas em
row-major), Softmax e RMSNorm. O dispatch seleciona AVX2/FMA em x86 com suporte,
NEON em ARM64 e escalar nas demais plataformas. O pacote amd64 é compilado
com base x86-64: AVX2/FMA fica em funções isoladas com atributo de alvo.
Não há BLAS nem runtime externo de computação; os kernels ligam apenas libc/libm.

GEMM usa blocos M=32, N=64 e K=64, microkernel AVX2/FMA 4×16 ou NEON 4×8,
reutilizando vetores B entre quatro linhas. As caudas são tratadas por vetores
menores e operações escalares. O fallback escalar também usa blocos para cache
e é compilado com autovetorização desabilitada. As funções não alocam memória
nem empacotam tensores. `sct_buffer_alloc` fornece scratch com `posix_memalign`
de 64 bytes, liberado por `free`. Entradas naturalmente alinhadas em 4 bytes
também funcionam, inclusive quando não alinhadas em 16/32/64 bytes.

Softmax subtrai o máximo e acumula exponenciais em precisão dupla. SIMD usa
redução de argumento e polinômio de grau 6 para exp no intervalo [-80,0];
caudas mais negativas usam libm em precisão dupla. RMSNorm calcula soma dos
quadrados em double, evitando overflow intermediário para entradas float32
grandes; aplicação da escala e pesos também é em double antes da conversão.
Não há aproximação de raiz recíproca. Subnormais seguem o modo FP do processo;
programas ligados com fast-math podem habilitar flush-to-zero.

Softmax/RMSNorm aceitam saída in-place, rejeitam sobreposição parcial, entradas
não finitas e tamanhos com overflow. RMSNorm exige pesos finitos e epsilon
positivo finito; rejeita resultado fora da faixa float32. Essas verificações
ocorrem antes de alterar a saída. GEMM verifica tamanhos e alias da saída com
A/B; segue aritmética float/FMA sem varredura de finitude nem detecção de
overflow aritmético. Dimensões vazias são no-op. Chamadas SIMD explícitas
devolvem ENOTSUP em hardware incompatível, inclusive para entradas vazias.

```sh
ssh root@subspace /usr/libexec/schipia/test_kernels
ssh root@subspace /usr/libexec/schipia/bench_kernels
ssh root@subspace 'valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=99 /usr/libexec/schipia/test_kernels'
```

Os testes usam referência independente double compilada sem fast-math,
contração FMA ou autovetorização. Tolerâncias: GEMM `2e-5 + 2e-6*sum_abs`,
Softmax `3e-7 + 1e-5*abs(ref)`, RMSNorm `3e-7 + 2e-6*abs(ref)`;
`sum_abs` inclui C inicial e os produtos acumulados. Essas são tolerâncias
dos testes, não uma prova de erro para todas as entradas possíveis.

O benchmark compara kernel manual, escalar e GEMM em C autovetorizada pelo GCC,
com o mesmo conjunto AVX2/FMA no servidor x86. Mede a mediana de cinco amostras,
confere todas as células e inclui formas quadradas, retangulares e M=1.
O baseline autovetorizado é um loop em blocos, não uma BLAS otimizada.
`make VEC_REPORT_FLAGS=-fopt-info-vec-optimized=build/gemm_autovec.vec deb`
registra o relatório GCC da vetorização. `make arm64-tests` apenas compila um
teste ARM64 estático para executar depois no servidor via QEMU; emulação não
mede desempenho NEON em hardware físico.

## Tokenizador e conversão

```sh
convert_vocab json tokenizer.json exemplo.sct_tokens --embedding-stride 4096
schipia-tokenize exemplo.sct_tokens 'olá mundo! olá'
schipia-tokenize exemplo.sct_tokens --file entrada.txt
convert_vocab sentencepiece modelo.vocab sentencepiece.sct_tokens
convert_vocab lines vocab.txt linhas.sct_tokens
```

`json` aceita mapas token→ID (`vocab.json` BPE), `model.vocab` e `vocab`
em objetos HuggingFace. Mapas preservam IDs uint32, inclusive zero. Arrays
Unigram `[token, score]` usam a posição como ID. `added_tokens` importa
`content` e `id`; entradas idênticas são idempotentes, conflitos são erros.
`sentencepiece` aceita `piece<TAB>score`, com IDs pela ordem das linhas.
`lines` aceita um token por linha, também na ordem; espaços são preservados,
CRLF é aceito e linhas vazias são rejeitadas. Tokens binários/NUL podem ser
importados por escapes JSON e tokenizados com `--file`.

São importados **vocabulários**, preservando os bytes literais, sem executar
normalização, pretokenização, regras BPE/WordPiece, scores Unigram ou regras
de tokens especiais. Marcadores como `▁`, `Ġ` e `##` continuam literais.
O algoritmo nativo é sempre maior prefixo guloso; não promete equivalência
com os IDs produzidos pelo pipeline original HuggingFace/SentencePiece.
Arquivos protobuf `.model` e listas `merges.txt` isoladas não são suportados.

Tokens têm 1–256 bytes. IDs duplicados para tokens diferentes e tokens com
IDs/offsets conflitantes são rejeitados. `--embedding-stride` calcula
`id * stride`, sem gerar embeddings; sem essa opção o offset é UINT64_MAX
(não associado). Não há fallback desconhecido: a CLI falha com o offset
do primeiro byte sem token e não imprime IDs parciais.

`OUTPUT.sct_tokens` é um diretório criado exclusivamente, sem sobrescrever
destinos existentes. Contém `vocab.bin`, `keys.index` e `ids.index`.
O arquivo de dados tem cabeçalho de 128 bytes e registros de 16 bytes mais
o token variável, alinhados em 8 bytes. Guarda ID uint32, comprimento uint16,
campo reservado uint16 e offset de embedding uint64. Os índices guardam
offsets relativos uint64 para os registros; callbacks recuperam as chaves
reais diretamente do mmap. A chave de índice de token tem 258 bytes com
comprimento e padding; o registro não duplica esse padding.

As buscas testam comprimentos candidatos em ordem decrescente na árvore,
filtrados por bitmap de comprimentos presentes. Não percorrem o vocabulário
nem alocam buffers temporários na heap. O custo é até 256 buscas por token;
este primeiro formato não implementa um cursor de prefixo nativo da árvore.
`sct_vocab_by_id` devolve os bytes no mmap sem cópia. Os dados são mapeados
somente para leitura na abertura; os índices precisam ser graváveis porque
a libschiptree mantém metadados de abertura compartilhados.

Importação mapeia a entrada e usa a interface streaming de libschipjson
(`Tags_open`/`Tjson_getk_next`/`Tjson_unescape`), sem DOM nem cópia integral.
Uma página anônima final garante o terminador NUL exigido pela biblioteca,
inclusive para arquivos de tamanho múltiplo da página. Há validação limitada
de sintaxe, UTF-8 e profundidade antes do parser permissivo; até 19 níveis.
A libschipjson é distribuída no pacote oficial `libschipbase` neste SDK.
O pacote exige glibc >= 2.38, conforme os símbolos usados pelas CLIs.

Um builder incompleto é removido no fechamento normal. Interrupção abrupta
pode deixar um diretório incompleto: a abertura recusa seu marcador incompleto;
remova o diretório antes de tentar novamente. A selagem sincroniza dados e
índices antes de marcar o vocabulário completo. Não há recuperação de crash
nem garantia transacional/power-loss. O formato nativo não é intercambiável
entre ABIs incompatíveis. Handles são por thread; múltiplos leitores usam
handles separados. Ponteiros devolvidos são válidos até fechar ou adicionar.

## Compilação local

```sh
make
make profile
make clean
make ARCHFLAGS=-march=x86-64 deb
```sh
make examples
make check
# Instruções completas em INSTALL.md.
```sh
scp dist/schipia-phase5_0.5.0_amd64.deb root@subspace:/tmp/
ssh root@subspace 'dpkg -i /tmp/schipia-phase5_0.5.0_amd64.deb'
ssh root@subspace /usr/libexec/schipia/test_store
ssh root@subspace /usr/libexec/schipia/test_tokenizer
ssh root@subspace /usr/libexec/schipia/test_converters.sh
ssh root@subspace '/usr/libexec/schipia/bench_store 20000'
ssh root@subspace /usr/libexec/schipia/bench_tokenizer
ssh root@subspace 'valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --trace-children=yes --error-exitcode=99 /usr/libexec/schipia/test_store'
```

O `.deb` depende de `libschiptree (>= 1.0.0+schip3)` e
`libschipbase (>= 0.5.15+schip3)`, distribuídas em https://deb.schip.com.br.
Instala as CLIs em `/usr/bin` e os testes/benchmarks em `/usr/libexec/schipia`.
O pacote fase5 substitui os anteriores, preservando seus programas de validação.
Gerar/instalar o pacote não publica no APT.
Build release local usa `-O3 -Wall -Wextra -Werror -march=native -ffast-math`;
o comando de empacotamento acima seleciona x86-64 básico para o servidor.

## Memória persistente da Fase 1: contrato e layout

`memory_store_open(path, initial_size)` cria `path.records` e `path.index`.
O primeiro armazena registros mapeados por `mmap`; o segundo é a árvore da
libschiptree. O callback recupera a chave real no arquivo de registros pelo
offset relativo de 64 bits usado como recid interno. O valor externo de 64 bits
é independente desse offset e aceita inclusive zero e UINT64_MAX.

Chaves binárias têm 1–256 bytes. O índice recebe comprimento em dois bytes
big-endian seguido da chave e padding zero: chaves com prefixos iguais e bytes
NUL permanecem distintas. O limite fixo e o padding são uma escolha inicial;
o tokenizador da Fase 2 usa registros variáveis num arquivo separado. Buscas usam
apenas buffers na pilha, sem alocação temporária na heap da aplicação.

O cabeçalho ocupa 64 bytes (magic, versão, posição usada). Os registros têm
272 bytes na ABI amd64: valor uint64_t, chave codificada de 258 bytes e padding.
Não há ponteiros persistidos. O arquivo cresce por duplicação e remapeamento;
o callback resolve offsets contra a base atual. `MADV_RANDOM` orienta os acessos.
O formato v1 é nativo e exige arquitetura/libc compatíveis com a libschiptree.

Há um único handle/processo por arquivo, protegido por `flock` exclusivo.
Handles não são compartilháveis entre threads. Inserções repetidas retornam
EEXIST; não há atualização nem remoção nesta fase. `memory_store_sync` permite
verificar explicitamente erros de persistência. `close` também sincroniza,
mas sua assinatura void não devolve falhas. Não há transação atômica entre os
dois arquivos, recuperação automática de crash ou garantia contra perda de
energia. Não reutilize arquivos interrompidos sem reconstruir o índice.

O benchmark mede inserção, busca, sincronização, criação, reabertura com cache
quente, tamanho lógico dos arquivos e RSS máximo cumulativo do processo.
Os arquivos temporários são criados em `/var/tmp`, para evitar o tmpfs de `/tmp`.
Não promete inicialização instantânea nem apresenta reabertura quente como
cold start. São verificados todos os valores recuperados. O fluxo aleatório
usa permutação multiplicativa sem colisões; o sequencial usa IDs crescentes.
O harness cobre crescimento, chaves binárias/limites, duplicatas, exclusão
de abertura simultânea e reabertura em processo novo via exec.

## Verificação da Fase 2

O `test_tokenizer` verifica a frase `olá mundo! olá` contra IDs de referência
`6 5 2 1`, além de Unicode/surrogates, NUL, limites, metadados, offsets de
embedding, conflitos, entradas inválidas, crescimento e persistência via exec.
`test_converters.sh` verifica as CLIs instaladas para todos os formatos e o
caso de JSON com tamanho exatamente igual a uma página. Execute com
`SCT_VALGRIND=1` para verificar todos os subprocessos conversor/tokenizador.

O `bench_tokenizer` cria 32 mil tokens, reabre o vocabulário e confere 2 milhões
de tokens. O tempo de construção exclui a sincronização; o tempo de tokenização
inclui a preparação das pequenas frases com snprintf e a conferência dos IDs.
O cenário tem tokens de mesmo comprimento e não representa o pior caso
de buscas por prefixos. A reabertura usa cache quente.

Referências de formatos: [componentes HuggingFace](https://huggingface.co/docs/tokenizers/main/en/components)
e [vocabulário SentencePiece](https://github.com/google/sentencepiece/blob/master/doc/options.md).
