# API de ajuda e programação — SchipIA 0.12.0

Base: **https://schipia.schip.com.br**. O aplicativo envia a dúvida para essa
central e recebe uma resposta documental, com a fonte e o contexto autorizado.
Não precisa instalar o modelo no computador ou navegador de cada usuário.

Os modelos Qwen de diálogo, busca e programação estão ativos na central pública
com SchipIA 0.12.0. O laboratório do subspace continua separado, em
`http://127.0.0.1:18093` naquele servidor, acessível pelo túnel descrito em
[Modelos](MODELOS.md). Atualizar o pacote em uma instância própria não ativa
os modelos automaticamente.

## Autenticação e contexto

Solicite ao administrador uma chave para seu aplicativo, versões, perfil e
organização. Todas as rotas `/v1/helpdesk/*` e `/v1/models` exigem:

```http
Authorization: Bearer CHAVE_DA_INTEGRACAO
Content-Type: application/json
```

Guarde a chave no **backend**. A interface do seu aplicativo chama seu próprio
backend; ele autentica o usuário e consulta a central. Escolha a chave de acordo
com os privilégios reais desse usuário. O corpo da requisição não concede
perfil administrativo nem troca a organização da chave.

O backend fornece `user_id` a partir da sessão autenticada: a central confia
nessa identificação, não autentica os usuários finais do seu aplicativo.
Não aceite um `user_id` arbitrário enviado pelo navegador. Para usar a página
da central diretamente, utilize uma chave pessoal autorizada para esse fim.

Todos os POSTs deste guia recebem o contexto:

| Campo | Exemplo | Utilização |
|---|---|---|
| `application` | `schipsave` | Aplicativo autorizado pela chave |
| `version` | `0.40.3` | Versão funcional, cadastrada na central |
| `screen` | `geral` | Tela ou área na qual o usuário está |
| `user_id` | `usuario-42` | Identificador estável da sessão autenticada |

Use identificadores com letras ASCII, números ou `_.:+-~`, sem espaços.
Consulte os aplicativos e telas em [HELPDESK.md](HELPDESK.md). O catálogo
completo está em `/usr/share/schipia/helpdesk/catalog.tsv` no servidor.
Não acrescente `role` ou `tenant` ao corpo: a chave já define esses valores.

## Perguntar

`POST /v1/helpdesk/answer`:

```json
{
  "application": "schipsave",
  "version": "0.40.3",
  "screen": "geral",
  "user_id": "usuario-42",
  "question": "Como verificar um backup?",
  "history": []
}
```

`history` é opcional e aceita até oito mensagens, com `role` (`user` ou
`assistant`) e `content`. O modelo usa o histórico recente que couber no contexto
para esclarecer perguntas e manter a continuidade. Não conte com memória
automática de sessão pelo `user_id`:
envie o contexto necessário a cada pergunta. Ao trocar aplicativo, versão ou
tela, reinicie o histórico exibido pela sua interface.

Exemplo para o backend, com a chave em um arquivo privado do curl:

```sh
# /caminho/privado/schipia.curl, permissão 0600:
# header = "Authorization: Bearer SUBSTITUA_PELA_CHAVE"
curl --fail-with-body --silent --show-error \
  --config /caminho/privado/schipia.curl \
  --header 'Content-Type: application/json' \
  --data '{"application":"schipsave","version":"0.40.3","screen":"geral","user_id":"usuario-42","question":"Como verificar um backup?","history":[]}' \
  https://schipia.schip.com.br/v1/helpdesk/answer
```

Trate os campos da resposta assim:

| Campo | O que fazer |
|---|---|
| `status` | `answered`: exiba a resposta; `not_found`: peça detalhes ou encaminhe ao suporte |
| `answer` | Texto em português; renderize Markdown com HTML desabilitado ou sanitizado, destacando blocos de código |
| `sources` | Lista com `id`, `title`, `url`, `original_url`, `application`, `version` |
| `dialogue_id` | Guarde junto da pergunta para enviar sua avaliação |
| `retained` | `false` indica que a conversa não foi retida para aprendizado |
| `model_assisted` | Indica participação do modelo na seleção ou geração da resposta |
| `learned_alias` | Indica uso de uma associação ensinada por avaliação positiva |
| `answer_mode` | `grounded_document`: trecho documental; `grounded_generation`: resposta elaborada a partir das fontes autorizadas |
| `finish_reason` | Quando presente, `stop`: conclusão; `length`: limite atingido, resposta possivelmente incompleta |
| `learning_candidate_id` | Quando presente e positivo, identifica uma contribuição pendente de revisão |
| `elapsed_ms` | Tempo de atendimento no serviço, sem a rede do cliente |

Não execute automaticamente comandos retornados no texto. A API orienta o
usuário a operar o aplicativo; não executa operações nele.

Para abrir uma fonte, faça um GET autenticado no `url` relativo retornado,
sempre na mesma base da central. Exemplos de rotas:
`/v1/helpdesk/source/ID_DOCUMENTAL` e `/v1/helpdesk/knowledge/ID_NUMERICO`.
O retorno inclui `text`. A primeira rota também traz `title` e `original_url`;
a segunda traz os campos da contribuição aprovada. Use o texto citado e o
título na interface. O `original_url` é a referência de origem do documento.

## Consultar os modelos disponíveis

`GET /v1/models`, com a mesma chave, lista os modelos da instância e suas rotas.
Exemplo no laboratório com túnel SSH aberto:

```sh
BASE_URL=http://127.0.0.1:18093
curl --fail-with-body --silent --show-error \
  --config /caminho/privado/schipia.curl "$BASE_URL/v1/models"
```

Os IDs são `schipia-helpdesk` (Qwen3-4B), `schipia-coder` (Coder-7B) e
`schipia-coder-fast` (Coder-1.5B), conforme os modelos instalados e ativados.
Sem os novos modelos ativos, a lista fica vazia. O modelo
de embeddings é usado pela busca, sem uma rota pública própria neste serviço.

## Pedir ajuda de programação

`POST /v1/helpdesk/code` aceita o mesmo contexto, autenticação e histórico:

```json
{
  "application": "schipsave",
  "version": "0.40.3",
  "screen": "geral",
  "user_id": "usuario-42",
  "model": "schipia-coder",
  "question": "Escreva uma função C11 que calcule o MDC e explique os casos com zero.",
  "history": []
}
```

Use `schipia-coder` para o modelo de 7B e `schipia-coder-fast` para o de 1.5B.
Sem `model`, o serviço usa o maior disponível; consulte `/v1/models` para
conhecer a instalação. Um nome inválido retorna HTTP 400. Os identificadores
de aplicativo, versão e tela precisam continuar autorizados pela chave.

Salve o corpo acima como `pergunta.json` no backend e envie:

```sh
curl --fail-with-body --silent --show-error --max-time 300 \
  --config /caminho/privado/schipia.curl \
  --header 'Content-Type: application/json' --data-binary @pergunta.json \
  "$BASE_URL/v1/helpdesk/code"
```

Exemplo ilustrativo de resposta:

```json
{
  "status": "answered",
  "model": "schipia-coder",
  "answer_mode": "code_generation",
  "answer": "Segue uma sugestão em C11...",
  "sources": [],
  "executed": false,
  "finish_reason": "stop",
  "elapsed_ms": 16000
}
```

Renderize `answer` como Markdown seguro, com destaque para blocos de código.
O código é uma sugestão: **a API não o executa nem altera o aplicativo**.
Revise e teste antes de usar. `sources` fica vazio nesse atendimento; não o
apresente como um procedimento confirmado pelo manual. `finish_reason=length`
indica que o limite de geração foi atingido; confira se o código está completo.

## Avaliar a resposta

`POST /v1/helpdesk/feedback`, pela mesma integração, com o mesmo contexto e
usuário da pergunta:

```json
{"application":"schipsave","version":"0.40.3","screen":"geral","user_id":"usuario-42","id":123,"helpful":true}
```

Substitua `123` pelo `dialogue_id` recebido. Use `false` para avaliação negativa.
O retorno de sucesso tem `status=recorded`. Uma avaliação positiva de resposta
documental associa a pergunta à fonte; uma negativa desativa a promoção.
Na resposta gerada, uma avaliação positiva cria uma contribuição pendente,
que exige revisão do curador antes de uso compartilhado. Respostas sem fonte não
aceitam avaliação. Não envie avaliação quando `dialogue_id` for zero.

## Acrescentar conhecimento

Na pergunta enviada a `/v1/helpdesk/answer`, iniciar por `aprenda:` ou
`guarde esta informação:` também cria uma sugestão pendente. O campo
`learning_candidate_id` identifica o registro; a publicação da informação
continua dependendo de revisão. Isso não retreina os pesos do modelo.

`POST /v1/helpdesk/contributions`, com o contexto da informação:

```json
{
  "application": "schipsave",
  "version": "0.40.3",
  "screen": "geral",
  "user_id": "usuario-42",
  "question": "Qual procedimento esta informação esclarece?",
  "title": "Título do procedimento confirmado",
  "text": "EXEMPLO: substitua este texto pela informação verificada no aplicativo antes de contribuir."
}
```

O exemplo é um molde, não um fato para cadastrar. Envie procedimentos
confirmados, sem senhas ou dados pessoais desnecessários. `text` aceita de 32
a 4.095 bytes UTF-8; `title`, de 3 a 255 bytes. O retorno informa `id` e
`status=pending`. Uma contribuição repetida do mesmo autor/contexto pode
retornar o registro existente com `duplicate=true` e seu estado atual.

## Revisar contribuições

Requer uma **chave de curador** para o aplicativo/versão e organização.
`POST /v1/helpdesk/pending` recebe os quatro campos de contexto. A resposta
tem `contributions`, com até uma pendência por chamada. Cada item inclui
`id`, `title`, `application`, `version`, `screen`, `role`, `question`, `text`
e `decision` (0 enquanto pendente). A listagem cobre o aplicativo/versão e
organização consultados; confira a tela e o perfil do item antes de decidir.

`POST /v1/helpdesk/review`:

```json
{"application":"schipsave","version":"0.40.3","screen":"geral","user_id":"curador-7","id":456,"decision":"approve"}
```

Substitua `456` pelo ID da contribuição; use `reject` para rejeitar ou retirar
uma informação já aprovada. A resposta informa `status=approved` ou `rejected`.
Consulte novamente `/pending` para avançar para a próxima pendência.

A aprovação torna a contribuição consultável no aplicativo, versão, tela,
perfil e organização originais. Ela não amplia permissões nem retreina pesos.
O registro e as decisões sobrevivem ao reinício do serviço.

## Disponibilidade, erros e limites

`GET /health` é público e retorna `status=ready`, nome do serviço e versão.

| HTTP | Tratamento pelo aplicativo |
|---|---|
| 200 | Leia `status`; `not_found` também usa HTTP 200 |
| 400 | Corrija JSON, campos ou valores inválidos |
| 401 | Chave ausente ou inválida |
| 403 | Contexto, autoria ou privilégio não autorizado |
| 404 | Rota ou fonte indisponível para essa chave |
| 503 | Armazenamento ou atendimento indisponível; registre e tente depois |

O corpo JSON tem limite de 32 KiB. Pergunta e conteúdo de cada mensagem de
histórico aceitam até 2.047 bytes. Esta versão atende uma requisição por vez;
mostre um estado de espera e impeça envios duplicados enquanto consulta.
O modelo de instruções aumenta o tempo de consulta em relação à busca
documental. Para verificar ou alterar o modo de operação, fale com o
administrador da central.

Em instâncias com os novos modelos, comece com **300 segundos** de timeout no
cliente e no proxy para contextos maiores e ajuste à sua carga. O limite não
garante que toda consulta termine nesse prazo. Mostre progresso e trate
interrupções: a geração em CPU pode levar minutos. Uma integração
antiga com timeout de 25 segundos pode precisar de ajuste antes de usar o laboratório.

Veja [Administração](ADMINISTRACAO.md) para configurar credenciais, importar
manuais, acompanhar a retenção e fazer backup.
