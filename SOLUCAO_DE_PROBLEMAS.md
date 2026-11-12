# Solução de problemas

Comece com `engine_run --version` e `engine_run --help`. Para suporte, informe
também o comando utilizado, a versão do pacote (`dpkg-query -W schipia`), o
formato do modelo e a mensagem de erro. Não envie seus contextos ou prompts
privados sem necessidade.

| Sintoma | Como resolver |
|---|---|
| `command not found` | Confira a instalação do pacote e se `/usr/bin` está no `PATH`. |
| Opção `--tree` ou `--sparse` não reconhecida | Atualize os executáveis; alguns builds 0.8.0 não incluem esses recursos. |
| Pacote indisponível no APT | Confira a origem em `apt-cache policy schipia`; solicite o `.deb` se a versão ainda não foi publicada. |
| Biblioteca não encontrada | Instale as dependências Schip e `libgomp1` pelo APT; não copie bibliotecas de versões diferentes manualmente. |
| `Permission denied` no vocabulário | Use a cópia própria preparada em [Instalação](INSTALACAO.md), com escrita permitida. |
| `File exists` ao usar `--new` | Para retomar, remova a opção; para outra história, escolha um caminho novo. |
| `Resource temporarily unavailable` | Outro processo pode estar usando o mesmo contexto. Feche-o antes de reabrir. |
| `No space left on device` no prompt/geração | Pode ser limite de tokens do contexto, além de falta de disco. Reduza `--tokens` ou inicie outra sequência. |
| Contexto recusado depois de trocar modelo | Use contexto novo com o modelo e o vocabulário correspondentes. |
| Conversão BF16 recusada | O modelo já pode ser BF16, ou os pesos não admitem conversão exata. Leia a mensagem; não force a conversão. |
| `Connection refused` no HTTP | Confira o processo, a porta e o endereço utilizados. |
| `/health` responde, mas geração falha | Confira caminhos, permissões, prefixo de contexto e limites do prompt; health não carrega o modelo. |

## Texto repetitivo ou sem coerência

Use um prefixo em português, por exemplo “Era uma vez...”. Experimente
`--temp 0.7 --top-p 0.9 --top-k 40 --repeat-penalty 1.1`. Compare com a configuração
gulosa padrão. Q4 e esparsidade positiva podem piorar a qualidade; volte ao BF16
e a `--sparse 0` para comparar. Nenhuma dessas opções garante ausência de repetição.

## Interrupção e reabertura

Uma interrupção durante gravação pode deixar o contexto marcado como incompleto
ou sujo (`Structure needs cleaning`). Não edite os arquivos nem remova marcadores
manualmente. Preserve uma cópia para diagnóstico e recomece em diretório novo,
com um prompt salvo. Não há comando de reparo ou recuperação automática.

## Servidor não volta a gerar após reiniciar

O prefixo escolhido pode colidir com contextos remanescentes. Pare o servidor e
use outro prefixo, como no exemplo de [HTTP](HTTP.md). Arquive ou remova somente
os contextos antigos que você identificou e já não precisa manter.

## Quer comparar desempenho?

Use o mesmo corpus e formato, feche outras cargas pesadas, compare 1 e 4 threads
e repita pelo menos três vezes. O número de tokens/s depende do equipamento,
comprimento do contexto e opções de geração. Uma diferença pequena numa única
execução não demonstra melhoria. Para o mesmo objetivo, confira também a memória
e a qualidade do texto, não apenas a velocidade.
