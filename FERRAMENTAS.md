# Ferramentas de utilização

Os exemplos usam `MODEL` e `STATE` de [Instalação](INSTALACAO.md). A ferramenta
principal de geração e memória está descrita em [Uso](USO.md).

## Tokenizar texto com o modelo instalado

```sh
schipia-tokenize "$MODEL/vocab.sct_tokens" 'Olá, São Paulo!'
schipia-tokenize "$MODEL/vocab.sct_tokens" --file entrada.txt
```

A saída é uma linha de IDs. O vocabulário do Tucano instalado usa as regras
correspondentes ao modelo; não remova os arquivos que acompanham esse vocabulário.

## Converter um vocabulário independente

```sh
convert_vocab json tokenizer.json meu-vocab.sct_tokens
convert_vocab sentencepiece modelo.vocab meu-sentencepiece.sct_tokens
convert_vocab lines vocab.txt minhas-linhas.sct_tokens
```

Cada destino é um diretório novo. `json` aceita vocabulários em mapas token→ID e
estruturas de vocabulário compatíveis de Hugging Face. `sentencepiece` recebe um
arquivo textual com token e score separados por tabulação. `lines` recebe um
token por linha, sem linhas vazias. IDs são preservados quando explícitos ou
atribuídos pela ordem das entradas, conforme o formato.

Essa conversão genérica importa somente o vocabulário. Não reproduz todo o
tokenizador original, normalizações, BPE, WordPiece ou regras Unigram. Ela não
substitui o importador específico de um modelo treinado. Um arquivo SentencePiece
binário `.model`, um GGUF ou um `merges.txt` isolado não são entradas suportadas.
Marcadores como `▁`, `Ġ` e `##` permanecem literais nessa conversão genérica.

Tokens têm de 1 a 256 bytes; conflitos de IDs ou tokens são rejeitados. A opção
avançada `--embedding-stride BYTES` associa um passo por ID, sem criar embeddings;
normalmente não é necessária para testar um vocabulário independente.

## Avaliar previsão de texto

```sh
OMP_NUM_THREADS=4 schipia-eval "$MODEL/model.sct_model" "$MODEL/vocab.sct_tokens" \
  /usr/share/schipia/fixtures/historias-pt.txt 8 128
```

Argumentos finais: número de histórias e máximo de tokens por história.
Padrões: 8 e 128. Arquivos próprios devem usar UTF-8 e separar histórias com a
linha `<|endoftext|>`. O vocabulário precisa corresponder ao modelo.

| Campo da saída | Como interpretar |
|---|---|
| `scored_tokens` | Quantidade de previsões efetivamente avaliadas. |
| `mean_nll`, `perplexity` | Erro de previsão; menor costuma ser melhor no mesmo corpus. |
| `top1` | Fração de próximos tokens acertados pela escolha mais provável. |
| `forward_tokens_s` | Taxa de processamento das previsões medidas. |
| `median_us`, `p95_us` | Latência mediana e percentil 95, em microssegundos. |
| `peak_rss_kib` | Pico de memória residente, em KiB. |
| `perf_available` | Se os contadores de hardware estavam disponíveis. |

Compare sempre o mesmo texto, modelo, limite e número de threads. Perplexidade
não mede criatividade, segurança infantil nem a qualidade de uma história inteira.
Contadores de ciclos do avaliador cobrem a thread principal; não representam a
soma do trabalho de todas as threads numa execução paralela.

Para comparar sem a aceleração de consultas, acrescente `--no-hot-cache` depois
dos dois argumentos numéricos. Builds com esparsidade também aceitam
`SCT_SPARSITY=0.3` no ambiente; compare com 0 e confira a qualidade, além da taxa.

## Administrar contexto diretamente por IDs

Para quem já tem IDs válidos do tokenizador:

```sh
schipia-context new "$MODEL/model.sct_model" "$STATE/contexto-ids" 7
schipia-context state "$MODEL/model.sct_model" "$STATE/contexto-ids" 7
schipia-context sequence "$MODEL/model.sct_model" "$STATE/contexto-ids" 8
schipia-context add "$MODEL/model.sct_model" "$STATE/contexto-ids" 7 675
```

`new` cria o diretório e a primeira sequência; `sequence` cria outro ID dentro
dele; `state` consulta; `add` acrescenta um ou mais IDs. No último exemplo, 675 é
um ID do vocabulário Tucano; para seu texto, obtenha os IDs com `schipia-tokenize`.
Para histórias, prefira `engine_run`, que já faz a conversão do texto.
Não misture contextos administrados diretamente por IDs com sessões textuais já
existentes: crie diretórios independentes para esses usos.

## Demonstrações sintéticas

`engine_run demo DIRETORIO [SEED]` e `schipia-context model ARQUIVO [SEED]`
criam modelos de demonstração sem treinamento. Servem para verificar comandos,
não para produzir histórias legíveis. Não os confunda com o Tucano fornecido.
