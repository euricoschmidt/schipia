# SchipIA — manual do usuário

Geração local e continuação de histórias em português, com sessões persistentes,
opções de amostragem, modelos em formatos menores e servidor HTTP de streaming.
Este manual acompanha a versão **0.8.1**.

## Comece aqui

1. [Instale o pacote e prepare o uso sem root](INSTALACAO.md).
2. [Gere sua primeira história e retome o contexto depois](USO.md).
3. [Escolha BF16, Q8 ou Q4 e conheça os recursos experimentais](MODELOS.md).

## Todos os recursos

| Guia | Conteúdo |
|---|---|
| [Instalação](INSTALACAO.md) | Debian, APT, pacote recebido, permissões e atualização. |
| [Uso no terminal](USO.md) | Geração, amostragem, threads, modo interativo, arquivos, sessões e memória associativa. |
| [Modelos](MODELOS.md) | Conversão BF16/Q8/Q4, bundle em árvore, esparsidade e importadores compatíveis. |
| [Servidor HTTP](HTTP.md) | Inicialização, health, streaming, campos aceitos e limites atuais. |
| [Ferramentas](FERRAMENTAS.md) | Tokenização, conversão de vocabulários, avaliação e contexto por IDs. |
| [Desempenho](DESEMPENHO.md) | Medições, memória, qualidade e escolha de configuração. |
| [Solução de problemas](SOLUCAO_DE_PROBLEMAS.md) | Diagnóstico de instalação, contexto, permissões, geração e servidor. |
| [Licença e componentes de terceiros](LICENCIAMENTO.md) | Termos de avaliação, produção, OEM e avisos preservados. |

O modelo fornecido é o Tucano-160M, pré-treinado em português. Ele continua um
texto iniciado pelo usuário; não é um modelo de chat nem uma fonte confiável de
fatos. O contexto fornecido comporta 2048 tokens. Amostragem pode reduzir a
repetição, mas não garante coerência de uma história inteira.

Este repositório contém somente documentação de utilização e textos de licença.
Os pacotes são fornecidos separadamente; a implementação da engine não integra
este repositório. Confira `engine_run --version` e `engine_run --help`: builds
anteriores identificados como 0.8.0 podem não oferecer `--sparse` e `--tree`.

Distribuição e dependências: <https://deb.schip.com.br>.
Contato comercial e suporte: **euricosalvesschmidt@gmail.com**.

Copyright (c) 1988 Eurico Schmidt. Todos os direitos reservados.
Leia o [contrato de licença](LICENSE) antes de utilizar o produto.
