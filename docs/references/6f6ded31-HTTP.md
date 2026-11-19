# Servidor HTTP com streaming

O servidor fornece continuação de texto por HTTP. Prepare `MODEL` e `STATE`
conforme [Instalação](f5304d96-INSTALACAO.md). Para iniciar em primeiro plano:

```sh
schipia-server "$MODEL/model.sct_model" "$MODEL/vocab.sct_tokens" 8080 \
  "$STATE/http-$(date +%Y%m%d-%H%M%S)" 4
```

Argumentos: modelo, vocabulário, porta, prefixo dos contextos e threads. Se
omitidos, os dois últimos usam `/tmp/schipia-server-cache` e 4 threads.
Escolha um prefixo novo ao reiniciar: cada requisição cria um contexto próprio,
e os arquivos de sessões anteriores permanecem no disco. Não há retomada de
uma conversa HTTP por ID de sessão.

O endereço de escuta é **0.0.0.0**. O servidor atual não oferece autenticação,
TLS ou seleção de endereço de escuta. Controle o acesso pela rede/firewall ou
por um proxy apropriado antes de disponibilizá-lo a outras máquinas. Ele atende
uma requisição de geração por vez.

## Consultar disponibilidade HTTP

```sh
curl -sS http://127.0.0.1:8080/health
```

Resposta HTTP 200 com `status`, `engine` e `version`. Esse endpoint confirma que
o servidor HTTP responde; a validação do modelo ocorre na requisição de geração.

## Pedir uma continuação

```sh
curl -N -X POST http://127.0.0.1:8080/v1/completions \
  -H 'Content-Type: application/json' \
  --data-binary '{"prompt":"Era uma vez um pássaro azul"}'
```

A resposta usa `Content-Type: text/event-stream`. Fragmentos de texto vêm em
eventos `data: ...`; o final é indicado por `data: [DONE]`. Esses fragmentos são
texto, não objetos JSON com `choices` ou `delta`.

## Contrato atual e limites

Esta interface é experimental e não implementa toda uma API de completions
compatível com outros fornecedores. Apenas `prompt` é lido no corpo. A geração
usa os valores fixos atuais: até 128 tokens, temperatura 0.7, top-p 0.9, top-k 40
e penalidade de repetição 1.15. Campos como `max_tokens`, `temperature`, `stream`
ou `model` não configuram a geração. Para escolher esses valores, use a CLI.

Envie requisições pequenas, com prompt simples, como no exemplo compacto acima.
O leitor atual não é um parser JSON geral: não conte com aspas escapadas,
quebras de linha escapadas ou variações de formatação no nome `"prompt":`.
O prompt tem limite de 4095 bytes; os limites de contexto do modelo também se
aplicam. A implementação não assegura leitura de corpos grandes ou fragmentados.

Erros de inicialização retornam HTTP 500; falha ao aceitar o prompt retorna 400;
rotas desconhecidas retornam 404. Depois que o streaming começou, `[DONE]` sozinho
não distingue conclusão normal de todos os tipos de erro de geração.
Se uma requisição com erro deixar um contexto que impeça as seguintes, pare o
servidor e reinicie com outro prefixo, preservando os dados antigos para diagnóstico.

## Encerrar e administrar os contextos

Use Ctrl+C no terminal. Se houver uma conexão ativa, aguarde seu encerramento.
Depois de parar o processo, arquive ou remova somente os contextos com o prefixo
escolhido para esse servidor. Em uma nova execução, escolha outro prefixo para
evitar colisão com arquivos remanescentes.


## Motor persistente e endpoint `/v1/chat/completions`

O `./bin/schipia-server` mantém o modelo e o cache KV **carregados em memória**
(`sct_engine_open`) entre requisições, alocando novas sequências via
`sct_engine_new_sequence` sem reiniciar processo a cada chamada e aproveitando
automaticamente o prefix caching em `libschiptree`.

### Inicialização

```sh
./bin/schipia-server model.sct_model vocab.sct_tokens 8080 /var/tmp/server.cache 4
```

### `POST /v1/chat/completions`

Compatível com clientes OpenAI (com ou sem streaming SSE):

```sh
curl -N http://127.0.0.1:8080/v1/chat/completions \
  -H 'Content-Type: application/json' \
  -d '{
    "chat_template": "chatml",
    "messages": [
      {"role": "system", "content": "Você é um assistente técnico."},
      {"role": "user", "content": "Resuma o que é KV cache."}
    ],
    "max_tokens": 128,
    "temperature": 0.7,
    "top_p": 0.9,
    "top_k": 40,
    "repeat_penalty": 1.1,
    "stream": true
  }'
```

Parâmetros JSON suportados em `/v1/completions` e `/v1/chat/completions`:
`prompt`, `messages`, `chat_template` / `format` (`chatml` ou `llama3`),
`max_tokens`, `temperature`, `top_p`, `top_k`, `repeat_penalty`, `seed` e `stream`.
