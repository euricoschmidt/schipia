# Modelos para diálogo, busca e programação — 0.10.0

Os perfis Qwen são opcionais e instalados separadamente do pacote `schipia`.
Os aplicativos consultam a API central; seus usuários não precisam baixar os
pesos. A instalação e o uso não exigem Python, PyTorch ou programas em C++.

## Escolher o perfil

| Instalação | Modelo | Formato padrão | Uso pela API |
|---|---|---|---|
| `qwen3` | Qwen3-4B-Instruct-2507 | Q8 | `schipia-helpdesk`, respostas sobre os manuais |
| `coder` | Qwen2.5-Coder-7B-Instruct | Q4 | `schipia-coder`, assistência de programação |
| `coder-fast` | Qwen2.5-Coder-1.5B-Instruct | Q8 | `schipia-coder-fast`, menor consumo e latência |
| `embedding` | Qwen3-Embedding-0.6B | Q8 | Busca por significado usada pelo helpdesk |

Prefira o Coder-7B quando a qualidade do código for prioritária. O perfil menor
economiza recursos, mas pode cometer mais erros. Compare as medições em
[Desempenho](DESEMPENHO.md); revise e teste qualquer código sugerido.

## Baixar e instalar

Instale primeiro `schipia=0.10.0` conforme [Instalação](INSTALACAO.md).
Reserve dezenas de GB para baixar e converter os quatro modelos completos.

```sh
sudo schipia-models install qwen3 /var/lib/schipia-models
sudo schipia-models install coder /var/lib/schipia-models
sudo schipia-models install coder-fast /var/lib/schipia-models
sudo schipia-models install embedding /var/lib/schipia-models
```

Uma instalação válida existente é reutilizada. Os diretórios de cada perfil
são `qwen3`, `coder7`, `coder` e `embedding`, respectivamente. Mantenha cada
diretório inteiro. Um quarto argumento opcional `4`, `8` ou `16` escolhe a
precisão; os exemplos usam as configurações validadas acima.

Cada modelo inclui registros `ORIGIN`, `LICENSE` e `SHA256SUMS`. Os quatro
Qwen têm licença Apache 2.0; preserve seus avisos ao redistribuí-los.
A licença da SchipIA permanece a mesma da libschiptree. O pacote também
acompanha os avisos da licença Unicode dos dados utilizados.

## Ativar em uma instância própria

Com o serviço parado e backup de seu estado, gere o índice da base desejada:

```sh
schipia-helpdesk semantic-index /CAMINHO/knowledge /var/lib/schipia-models /CAMINHO/semantic
```

Configure estas variáveis no ambiente do serviço antes de iniciá-lo:

```text
SCHIPIA_MODELS=/var/lib/schipia-models
SCHIPIA_SEMANTIC=/CAMINHO/semantic
```

A conta do serviço deve ler os modelos e o índice e gravar em seu próprio
diretório de estado. Consulte [Administração](ADMINISTRACAO.md) para configurar
a instância, as credenciais e o serviço. Sem a ativação desses perfis, o
atendimento anterior continua disponível.

Após atualizar os manuais ou o modelo de busca, regenere o índice semântico.
Uma execução interrompida pode ser retomada com o mesmo comando, base e modelo.

## Acessar o laboratório no subspace

Os quatro perfis estão instalados no laboratório permanente em
`/srv/schipia-lab/0.10.0`, com serviço `schipia-lab-010.service` e estado
separado em `/var/lib/schipia-lab-010`. A API escuta em `127.0.0.1:8091`
no servidor. A central pública em `https://schipia.schip.com.br` conserva
seu perfil anterior; publicar o pacote não ativa Qwen nessa central.

Para testar de um computador com acesso SSH autorizado:

```sh
ssh -N -L 18091:127.0.0.1:8091 root@subspace
```

Mantenha o túnel aberto e, em outro terminal, use
`BASE_URL=http://127.0.0.1:18091`. As chamadas ainda exigem uma chave autorizada
do aplicativo. Não copie credenciais para código público nem para o navegador.

## Usar os modelos

- `GET /v1/models`: consultar os perfis disponíveis na instância.
- `POST /v1/helpdesk/answer`: perguntar usando manuais autorizados e histórico.
- `POST /v1/helpdesk/code`: pedir código com `model` igual a `schipia-coder`
  ou `schipia-coder-fast`; sem o campo, prefere o maior disponível.

Veja exemplos e autenticação em [API](API.md). Use timeout de pelo menos
180 segundos como ponto de partida em CPU. `answer` pode conter Markdown;
`finish_reason=length` indica resposta truncada. Na programação,
`executed=false`: a central não executa o código.

Envie o histórico recente a cada pergunta. Informações iniciadas por `aprenda:`
ou `guarde esta informação:` e respostas geradas avaliadas como úteis podem
criar contribuições pendentes. Um curador precisa aprová-las antes de serem
usadas em outros diálogos. Isso não retreina os pesos dos modelos.

## Modelos legados para uso pelo terminal

Os exemplos pressupõem `MODEL` e `STATE` definidos em [Instalação](INSTALACAO.md).
Use diretórios novos para conversões e contextos diferentes para cada modelo.
Nenhuma conversão exige baixar novamente o checkpoint fornecido no pacote.

## Qual formato escolher

| Formato | Quando usar | Tamanho aproximado do Tucano fornecido |
|---|---|---|
| BF16 | Padrão distribuído; preserva os pesos de origem. | 325 MB |
| Q8 | Menos memória e maior velocidade, com pequena alteração numérica. | 173 MB |
| Q4 | Priorizar tamanho e velocidade; verificar a qualidade do texto. | 92 MB |

Tamanho do arquivo não é o consumo total de memória: o programa também precisa
de espaço para o contexto. Q8 e Q4 alteram os pesos. Resultados de um formato
não devem ser tratados como idênticos aos de outro.

## Criar Q8 ou Q4

```sh
mkdir -p "$STATE/modelos/q8" "$STATE/modelos/q4"
schipia-pack --q8 "$MODEL/model.sct_model" "$STATE/modelos/q8/model.sct_model"
cp -r "$MODEL/vocab.sct_tokens" "$STATE/modelos/q8/"
schipia-pack --q4 "$MODEL/model.sct_model" "$STATE/modelos/q4/model.sct_model"
cp -r "$MODEL/vocab.sct_tokens" "$STATE/modelos/q4/"
```

Para usar o Q4, passe `"$STATE/modelos/q4/model.sct_model"` e
`"$STATE/modelos/q4/vocab.sct_tokens"` ao `engine_run`, com um contexto novo.
O mesmo vale para Q8. Prefira partir do BF16 original em vez de quantizar
novamente um modelo que já sofreu redução de precisão.

Sem opção de formato, `schipia-pack ORIGEM DESTINO` tenta converter para BF16
sem perder informação. Se os pesos não puderem ser representados exatamente,
a operação falha. Um modelo que já está em BF16 não precisa dessa conversão.

## Bundle em árvore e alinhamento de páginas

Requer executáveis com `--tree` na ajuda de `schipia-pack`. Para criar um bundle
Q4, primeiro faça a conversão Q4 acima e depois empacote em árvore:

```sh
schipia-pack --tree --page-align "$STATE/modelos/q4/model.sct_model" \
  "$STATE/modelos/q4-tree"
cp -r "$MODEL/vocab.sct_tokens" "$STATE/modelos/q4-tree/"
```

`--q4`, `--q8` e `--tree` são alternativas: não as combine na mesma chamada.
`--page-align` é usado com `--tree`. O diretório resultante precisa ser mantido
inteiro, incluindo todos os arquivos `weights.*`; não copie apenas
`model.sct_model`. Para carregar o bundle e seu índice, passe o diretório como
argumento de modelo. Use um destino novo para evitar sobrescrever dados existentes.
O procedimento documentado e validado aqui usa Q4; mantenha o BF16 no formato
padrão, sem aplicar esse procedimento de alinhamento a ele.

O bundle é utilizado pelo mesmo comando de geração:

```sh
engine_run generate "$STATE/modelos/q4-tree" \
  "$STATE/modelos/q4-tree/vocab.sct_tokens" "$STATE/historia-tree" 1 \
  --new --prompt 'Era uma vez um pássaro azul' --tokens 64 --threads 4
```

## Esparsidade experimental

Se a ajuda de `engine_run` listar `--sparse`, você pode acrescentar
`--sparse 0.3` à geração Q4/Q8. Comece comparando com `--sparse 0`, em contextos
separados. Mantenha o mesmo limiar durante toda a utilização de cada contexto.

Um limiar positivo pode mudar a saída e piorar a qualidade; economizar acessos
não garante reduzir o tempo. O valor 0 é o padrão e desativa esse comportamento.
Escolha a configuração depois de medir sua própria tarefa. Não há promessa de
ganho universal para o modelo fornecido.

## Importar checkpoints compatíveis

Para o perfil auditado Tucano-160M, os três arquivos de origem devem pertencer à
mesma revisão: `6331cbef9933c05d8d0c8ae5ee23ed2570f06571` de
[TucanoBR/Tucano-160m](https://huggingface.co/TucanoBR/Tucano-160m).

```sh
schipia-import-tucano model.safetensors config.json tokenizer.json native-pt
```

A saída `native-pt` deve ser nova. O importador não aceita qualquer modelo de
Hugging Face, nem adapta automaticamente tokenizadores diferentes. Reserve
espaço para os arquivos de origem, temporários e modelo convertido. Depois, a
conversão para BF16 usa `schipia-pack`, mantendo o vocabulário correspondente.

Também existe `schipia-import-llama CHECKPOINT.bin TOKENIZER.bin DIRETORIO` para
checkpoints FP32 originais llama2.c compatíveis. Não é um leitor de GGUF. O
modelo e o tokenizador precisam corresponder; trocar apenas um deles não é
uma forma válida de alterar o idioma ou a capacidade do modelo.


## Importação direta de modelos GGUF (`schipia-import-gguf`)

O utilitário `./bin/schipia-import-gguf` converte arquivos **GGUF v2/v3**
(LLaMA, Mistral, Qwen2, Gemma, DeepSeek, entre outros) diretamente para
`model.sct_model` (Q4) e `vocab.sct_tokens` (BPE):

```sh
./bin/schipia-import-gguf modelo.gguf ./saida_modelo --max-context 4096
```

Recursos suportados na importação:
- **Tipos de tensor**: `F32`, `F16`, `BF16`, `Q8_0`, `Q4_0` e `Q4_K` (requantizados diretamente para blocos Q4 da SchipIA).
- **Arquitetura**: Grouped-Query Attention (**GQA**, `kv_heads != heads`), `rope_theta` configurável, `rms_eps` configurável, até 48 camadas (`SCT_MAX_LAYERS`), `dim <= 8192`, `hidden <= 32768`, `vocab <= 262144` e contexto até `131072` tokens.
- **Tokenizador**: extração automática de tokens e merges BPE dos metadados GGUF.

## Bundle em árvore, alinhamento de página e deduplicação (`--tree --page-align`)

```sh
./bin/schipia-pack --q4 --tree --page-align model.sct_model ./modelo_tree_dir
```

- `--tree`: cria diretório indexado por `libschiptree` para lookup rápido de clusters.
- `--page-align`: alinha cada cluster de 32 neurônios em fronteiras de **4096 bytes**, reorganiza `SCT_DOWN` em layout **cluster-major** (leitura sequencial por cluster ativo) e aplica **deduplicação content-addressed** de clusters idênticos.
