# Diálogo, busca semântica e programação

O pacote SchipIA 0.10.0 mantém a consulta aos manuais e acrescenta modelos
opcionais para elaborar respostas, compreender perguntas semelhantes e ajudar
na programação. Os pesos são instalados separadamente do pacote de execução.
A instalação e o uso não exigem Python, PyTorch ou programas em C++.

## Instalar os modelos

Reserve espaço para os pesos baixados e para os modelos convertidos. Os quatro
perfis completos exigem dezenas de GB durante a instalação. O laboratório no
subspace usa CPU; respostas longas podem levar dezenas de segundos.

```sh
sudo schipia-models install qwen3 /var/lib/schipia-models
sudo schipia-models install coder /var/lib/schipia-models
sudo schipia-models install coder-fast /var/lib/schipia-models
sudo schipia-models install embedding /var/lib/schipia-models
```

Perfis disponíveis:

| Perfil | Modelo | Uso |
|---|---|---|
| qwen3 | Qwen3-4B-Instruct-2507 | Respostas em português sobre os manuais |
| coder | Qwen2.5-Coder-7B-Instruct | Assistência de programação |
| coder-fast | Qwen2.5-Coder-1.5B-Instruct | Programação com menor latência |
| embedding | Qwen3-Embedding-0.6B | Busca por significado |

Os modelos têm licença Apache 2.0. Cada instalação registra a origem, a revisão
e as somas SHA-256. A licença do projeto SchipIA permanece a mesma da libschiptree.

## Ativar em uma instância

Defina `SCHIPIA_MODELS` com o diretório dos modelos. Gere o índice semântico
para a base utilizada pela instância:

```sh
schipia-helpdesk semantic-index /CAMINHO/knowledge /var/lib/schipia-models /CAMINHO/semantic
```

Defina também `SCHIPIA_SEMANTIC=/CAMINHO/semantic` no ambiente do serviço.
O usuário do serviço precisa ler os modelos e o índice. A memória de diálogos
continua no diretório de estado escolhido na inicialização. Não reutilize a
memória de produção em uma instância de laboratório.

Regenere o índice após atualizar os manuais ou o modelo de busca. Uma execução
interrompida pode ser retomada com o mesmo comando, base e modelo.

Sem essas variáveis, a consulta documental continua disponível. Consulte
`INTEGRACAO.md` para credenciais, contexto, versões e permissões.

## Usar pela API

As rotas continuam exigindo a chave de acesso do aplicativo. Mantenha a chave
no backend. Use o endereço da instância desejada como `BASE_URL`.

- `GET /v1/models`: lista os perfis disponíveis.
- `POST /v1/helpdesk/answer`: responde com os documentos autorizados e o histórico.
- `POST /v1/helpdesk/code`: ajuda em programação; aceita `model` igual a
  `schipia-coder` ou `schipia-coder-fast`.

Exemplo de corpo para programação:

```json
{
  "application": "meuapp",
  "version": "1.0.0",
  "screen": "geral",
  "user_id": "usuario42",
  "model": "schipia-coder",
  "question": "Escreva uma função C11 que calcule o MDC e explique os casos com zero.",
  "history": []
}
```

Envie `Authorization: Bearer SUA_CHAVE` e `Content-Type: application/json`.
Desde a versão 0.14, programação usa geração assíncrona. A resposta inicial
contém `job_id`, `status=running` ou `queued` e o modelo selecionado. Pedidos
de programas enviados a `/answer` também usam o Coder automaticamente.

Consulte `POST /v1/helpdesk/code/poll` com o mesmo contexto autenticado,
`job_id` e `offset=0`. Acrescente `chunk` à resposta e use o novo `offset`
na próxima consulta. Continue até `status=completed` ou `failed`. As partes
são paginação de transporte: não representam um tamanho máximo da resposta.
Repetir uma consulta com o mesmo offset permite retomar sem perder conteúdo.
Somente o mesmo usuário, cliente, aplicativo, versão, tela e perfil têm acesso.

Para interromper, envie o contexto e `job_id` a
`POST /v1/helpdesk/code/cancel`. O texto parcial permanece disponível.
Cada consulta HTTP é curta; a geração continua entre consultas.

O campo `answer` contém Markdown. `answer_mode` informa se houve geração ou
resposta documental. `sources` contém as fontes autorizadas; os links de fontes
continuam sujeitos às permissões da API. No perfil de programação, `executed`
é sempre `false`: a SchipIA não compila nem executa o código solicitado.
Na programação, `complete=true` e `finish_reason=stop` indicam conclusão pelo
modelo. Cancelamento, erro e interrupção são informados explicitamente.
Não há teto configurado de tokens, bytes ou tempo total da geração de código.
O histórico integral é guardado no trabalho; a atenção usa janelas do modelo.
`context_compactions` informa quando foi necessário ajustar essas janelas.
Revise e teste o código antes de utilizá-lo: conclusão não garante correção.

## Histórico e informações úteis

Envie até oito mensagens anteriores, com `role` igual a `user` ou `assistant`
e `content` com o texto. A instância utiliza o contexto recente que couber no
limite do modelo. A pergunta atual deve permanecer no campo `question`.

O feedback útil em respostas documentais reforça a associação com sua fonte.
Respostas geradas marcadas como úteis criam sugestões pendentes de revisão.
Uma frase
iniciada por `aprenda:` ou `guarde esta informação:` cria uma sugestão pendente;
`learning_candidate_id` informa seu identificador. Um curador deve revisar e
aprovar antes de ela poder responder a outros diálogos. As sugestões não alteram
os pesos dos modelos. Aplicativo, versão, organização, perfil e tela continuam
delimitando quais informações podem ser usadas.
