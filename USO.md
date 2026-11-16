# Histórias, sessões e memória

Para perguntar sobre seus aplicativos na central de ajuda 0.9.0, consulte
[HELPDESK.md](HELPDESK.md). Para integrar esse atendimento ao seu sistema,
use [API.md](API.md). As instruções abaixo tratam da geração local no terminal.

Prepare `MODEL` e `STATE` como descrito em [Instalação](INSTALACAO.md).
O modelo fornecido continua textos em português. Comece com um trecho de
história; ele não é um assistente treinado para obedecer instruções de chat.

## Primeira história

```sh
engine_run generate "$MODEL/model.sct_model" "$MODEL/vocab.sct_tokens" \
  "$STATE/historia-ana" 1 --new \
  --prompt 'Era uma vez uma menina chamada Ana que encontrou uma chave no jardim.' \
  --tokens 64 --threads 4 --temp 0.7 --top-p 0.9 --top-k 40 \
  --repeat-penalty 1.15 --seed 42
```

A saída contém a continuação; não repete o prompt. As métricas aparecem na saída
de erro. `--new` exige uma pasta de contexto que ainda não exista. Para outra
história independente, escolha outro nome, como `historia-pedro`.

O número `1` identifica a sequência. `MODEL` aponta para o modelo; `STATE` guarda
seus contextos. O limite do Tucano fornecido é **2048 tokens**, somando todos os
prompts e as continuações da sequência. Tokens são partes de palavras, não uma
contagem de palavras ou caracteres.

## Continuar depois de fechar o programa

Reutilize os mesmos caminhos e identificador, sem `--new` e sem repetir o prompt:

```sh
engine_run generate "$MODEL/model.sct_model" "$MODEL/vocab.sct_tokens" \
  "$STATE/historia-ana" 1 --tokens 32 --threads 4 \
  --temp 0.7 --top-p 0.9 --top-k 40 --repeat-penalty 1.15 --seed 43
```

O texto anterior permanece no contexto. As opções de geração devem ser
informadas em cada chamada; não são preferências salvas automaticamente.
A retomada preserva o contexto, mas não salva o estado do sorteador: dividir uma
geração com amostragem em duas chamadas pode mudar o resultado.

Para consultar quantos tokens já foram consumidos:

```sh
engine_run inspect "$MODEL/model.sct_model" "$MODEL/vocab.sct_tokens" \
  "$STATE/historia-ana" 1
```

Outra sequência no mesmo diretório pode ser criada com `--new-sequence`, um ID
ainda não usado e um novo prompt. Ela tem seu próprio histórico, mas compartilha
o diretório de dados. Use um processo por diretório de contexto de cada vez.

## Modo interativo

```sh
engine_run interactive "$MODEL/model.sct_model" "$MODEL/vocab.sct_tokens" \
  "$STATE/interativo" 1 --new --tokens 32 --threads 4 \
  --temp 0.7 --top-p 0.9 --repeat-penalty 1.15
```

Digite um prefixo de história e pressione Enter. Cada nova linha acrescenta texto
à sequência; não inicia uma história independente. Comandos disponíveis:

| Comando | Uso |
|---|---|
| `:state` | Mostrar o estado da sequência. |
| `:generate` | Continuar sem acrescentar outro prompt. |
| `:quit` | Encerrar a sessão. |

Para reabrir a mesma sessão, execute novamente sem `--new`.

## Opções de geração

| Opção | Efeito e padrão |
|---|---|
| `--tokens N` | Máximo de tokens novos nesta chamada; padrão 16. Pode terminar antes. |
| `--threads N` | Número de threads de cálculo. Use 1 ou 4 como pontos de comparação. |
| `--temp T` | Temperatura. Padrão 0: escolha determinística; valores positivos habilitam sorteio. |
| `--top-p P` | Limitar os candidatos pela probabilidade acumulada. Use `0 < P <= 1`; padrão 1. |
| `--top-k K` | Limitar aos K candidatos mais prováveis; padrão 0 desativa esse limite. |
| `--repeat-penalty P` | Penalizar repetição; padrão 1 desativa. Experimente 1.05 a 1.15. |
| `--seed N` | Semente do sorteio para uma chamada. Use um inteiro positivo para testes reproduzíveis. |
| `--eos ID` | Escolher o ID que encerra a geração. Normalmente use o padrão do modelo. |
| `--scalar` | Usar a execução escalar, útil para diagnóstico. |
| `--no-hot-cache` | Desativar a aceleração de consulta do contexto, útil para comparar desempenho. |
| `--sparse T` | Recurso experimental; veja [Modelos](MODELOS.md). Padrão 0. |

Para a configuração gulosa básica, use `--temp 0 --repeat-penalty 1`. Aumentar a
temperatura ou a penalidade não garante uma história melhor; penalidade forte
pode deixar o texto estranho. Não há treinamento nem aprendizado dos pesos ao
salvar um contexto.

## Prompt em arquivo e formatos de saída

`--prompt-file arquivo.txt` substitui `--prompt` no comando `generate`. O arquivo
deve conter o prefixo em UTF-8. Não use as duas opções juntas. `--tokens 0` permite
apenas acrescentar o prompt ao contexto, sem produzir continuação.

O modelo treinado usa por padrão texto com controles escapados. Escolha:

| Formato | Uso |
|---|---|
| `--format escaped` | Preserva acentos; mostra controles como `\n` e `\t`. |
| `--format raw` | Texto literal, com quebras de linha; sem terminador extra. |
| `--format ids` | IDs dos tokens separados por espaço. |

Para salvar uma continuação, acrescente ao comando `--format raw > historia.txt
2> metricas.log`. Isso salva apenas o trecho gerado nessa chamada; o histórico
persistente continua no diretório de contexto.

## Memória associativa

```sh
engine_run memory-put "$STATE/fatos" 'personagem:Ana' 42
engine_run memory-get "$STATE/fatos" 'personagem:Ana'
```

Essa memória associa uma chave a um número inteiro sem sinal de 64 bits. Não
armazena uma resposta textual e não faz o modelo incorporar o fato à história
automaticamente. Chaves devem ter de 1 a 256 bytes. Inserções são exclusivas:
uma chave já existente provoca erro, sem sobrescrever seu valor.

## Guardar seus dados

Feche os programas antes do backup. Copie o diretório de contexto inteiro e,
para memória associativa, todos os arquivos com o prefixo escolhido. Guarde
também o modelo e o vocabulário correspondentes, ou uma identificação da versão.
Não edite arquivos internos. Consulte [Solução de problemas](SOLUCAO_DE_PROBLEMAS.md)
se uma interrupção impedir a reabertura.


## Templates de chat (`--chat` e `--system`)

Para modelos instruídos ou conversacionais (como Qwen2, Llama-3, Mistral/ChatML),
use `--chat` e `--system` em `engine_run`:

```sh
./bin/engine_run generate model.sct_model vocab.sct_tokens /var/tmp/chat.cache 1 \
  --new --chat chatml --system "Você é um assistente técnico conciso." \
  --prompt "Explique mmap em C em 3 linhas." \
  --tokens 128 --temp 0.7 --top-p 0.9 --top-k 40 --repeat-penalty 1.1
```

Formatos suportados por `--chat`:
- `chatml`: `<|im_start|>system ... <|im_end|>`, `<|im_start|>user ... <|im_end|>`, `<|im_start|>assistant`.
- `llama3`: `<|begin_of_text|><|start_header_id|>...<|end_header_id|>`.

## Execução esparsa, Out-of-Core e Prefix Caching

- `--sparse <limiar>`: ativa a poda dinâmica de clusters FFN (32 neurônios por cluster).
- `--out-of-core`: habilita `madvise(MADV_WILLNEED)` coalescido em lote sobre clusters ativos por camada, otimizando demand-paging direto do SSD/NVMe (recomendado com bundle `--tree --page-align`).
- `--no-hot-cache`: desativa a retenção em RAM de clusters quentes quando desejado.
- **Prefix Caching persistente**: o cache KV (`.sct_cache`) indexa automaticamente blocos de 8 tokens por hash na `libschiptree` (`kind=3`). Prompts e system prompts repetidos reutilizam o KV cache entre chamadas e sessões sem recomputar o prefill.
