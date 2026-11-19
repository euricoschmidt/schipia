# Integração com a central de ajuda 0.9.0

Servidor: https://schipia.schip.com.br. A página inicial permite testar perguntas,
consultar fontes, avaliar respostas e enviar contribuições. Use uma chave própria
do contexto autorizado; nunca distribua uma chave administrativa compartilhada.

## Consultar a API

O backend do aplicativo envia `Authorization: Bearer CHAVE` e
`Content-Type: application/json`. Guarde a chave no backend. O aplicativo deve
autenticar seu usuário antes de atribuir `user_id`; a central confia nessa
identificação enviada pela integração autorizada. A chave limita aplicativo,
versões, perfil e organização. Separe as chaves de organizações diferentes.

`POST /v1/helpdesk/answer` recebe:

```json
{"application":"schipsave","version":"0.40.3","screen":"geral","user_id":"usuario-42","question":"Como verificar um backup?","history":[]}
```

`history` é opcional; quando usado, contém objetos `role` (`user` ou `assistant`)
e `content`. O histórico de programação é armazenado integralmente; não corte
as respostas anteriores ao solicitar correções ou continuação de um programa.
Pedidos de programação podem retornar `job_id`: nesse caso, siga o fluxo de
consulta e cancelamento descrito em `MODELOS.md`, em vez de esperar `answer`
na resposta inicial.
Use as versões e telas do catálogo instalado em
`/usr/share/schipia/helpdesk/catalog.tsv`; guias somente de instalação usam
`screen=instalacao` e perfil administrativo.

Uma resposta traz `status=answered` ou `not_found`, `answer`,
`sources`, `dialogue_id`, `retained`, `answer_mode` e `model_assisted`.
Exiba o texto e as fontes. Consulte o `url` interno de cada fonte com a mesma
autenticação. Não trate uma recusa como instrução para executar algo.
O texto é um trecho documental; o modelo de instruções auxilia a seleção.

## Avaliações e conhecimento novo

`POST /v1/helpdesk/feedback` recebe os campos `application`, `version`, `screen`,
`user_id`, `id` (o `dialogue_id` retornado) e `helpful` (booleano).
A avaliação deve corresponder ao mesmo usuário e contexto da
resposta. Avaliação positiva ensina uma associação entre pergunta e fonte;
negativa desativa essa promoção.

`POST /v1/helpdesk/contributions` recebe os mesmos campos de contexto da
pergunta, mais `question`, `title` e `text`. O texto precisa ter entre 32 e 4.095
bytes. A contribuição começa pendente. Não inclua senhas ou dados pessoais
desnecessários. Contribuições duplicadas retornam o registro existente.

Um curador consulta `POST /v1/helpdesk/pending` com `application`, `version`,
`screen` e `user_id` e recebe uma pendência por vez do aplicativo/versão e
organização consultados. Decide em `POST /v1/helpdesk/review` enviando esses
campos, `id` da contribuição e `decision` (`approve` ou `reject`).
A autorização de curadoria é separada das
chaves comuns. Consulte a contribuição aprovada pelo endereço
`/v1/helpdesk/knowledge/ID`. A aprovação não retreina os pesos: acrescenta
conhecimento recuperável no contexto original da contribuição.

## Administração do servidor

Serviço: `schipia-helpdesk.service`. Ele escuta somente em `127.0.0.1:8090`;
o Nginx oferece HTTPS. `GET /health` verifica disponibilidade sem revelar chaves.

Credenciais ficam em `/etc/schipia/clients.tsv`, permissão 0600, proprietário
`schipia`. Cada linha tem sete campos separados por TAB:

```text
identificador  chave_hex_64  aplicativo  versoes_separadas_por_virgula  perfil  organizacao  curador_0_ou_1
```

Gere uma chave com `schipia-helpdesk keygen`, capture-a diretamente em arquivo
privado e reinicie o serviço após alterar o cadastro. Não coloque esse arquivo
em repositórios ou logs. A lista não secreta de perfis provisionados fica em
`/etc/schipia/profiles.tsv` no servidor.

Estado persistente: `/var/lib/schipia-helpdesk/`. Pare o serviço antes de copiar
o diretório inteiro para backup. Preserve também as credenciais e o pacote usado.
A memória suporta 65.536 eventos por diretório; acompanhe espaço e `retained`.
Os registros incluem perguntas, respostas e contribuições dos usuários.

Para importar manuais, use um manifesto TSV com as colunas `id`, `application`,
`version`, `screen`, `roles`, `file`, `url`, nesta ordem. Consulte o manifesto
fornecido no projeto como exemplo. Execute `schipia-helpdesk ingest MANIFESTO NOVA_BASE`
em um diretório novo. Pare o serviço, faça backup da base anterior e troque
`/var/lib/schipia-helpdesk/knowledge` pela nova base, com proprietário `schipia`.
Mantenha a memória existente para preservar contribuições e avaliações.
Instalações de pacote preservam uma base já existente; atualizar o pacote não
substitui automaticamente seus manuais personalizados.

## Validação

No servidor: `/usr/libexec/schipia/run_checks.sh --valgrind --benchmarks`.
O pacote inclui perguntas reais derivadas dos manuais para validar respostas,
recusas, versões e permissões, além da comparação numérica do modelo com a
referência. Esses testes não substituem a avaliação com chamados reais de cada
aplicativo. Para perguntas de instalação adicionais, use
`schipia-helpdesk evaluate BASE ESTADO CASOS.tsv [MODELO VOCABULARIO]`.
