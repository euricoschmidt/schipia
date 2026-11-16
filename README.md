# SchipIA — central de ajuda e distribuição binária

Versão atual pelo APT: **0.10.0**, para Debian 13 / amd64.
A central está disponível em **https://schipia.schip.com.br**, com HTTPS,
autenticação e uma base compartilhada de documentação dos aplicativos.

Faça perguntas, consulte as fontes e avalie as respostas. As avaliações ajudam
a encontrar procedimentos em perguntas futuras. Informações novas podem ser
contribuídas e passam a ser utilizadas após aprovação de um curador.
O aprendizado fica na base persistente; os pesos não são retreinados a cada conversa.

A versão 0.10.0 acrescenta modelos opcionais Qwen para diálogo, busca semântica
e programação, incluindo `POST /v1/helpdesk/code`. Os pesos são baixados
separadamente. A atualização do pacote preserva o atendimento existente.
Os novos modelos permanecem ativados no **laboratório do subspace**, separado
da central pública. Veja [Modelos](MODELOS.md) para instalar, acessar o laboratório
e escolher entre os perfis disponíveis.

| O que você precisa fazer | Guia |
|---|---|
| Usar a central, perguntar e contribuir | [Central de ajuda](HELPDESK.md) |
| Integrar o prompt de ajuda ao seu aplicativo | [API do helpdesk](API.md) |
| Instalar e atualizar a versão 0.10.0 | [Instalação](INSTALACAO.md) |
| Usar os novos modelos e a API de programação | [Modelos](MODELOS.md) e [API](API.md) |
| Administrar uma central própria e seus manuais | [Administração](ADMINISTRACAO.md) |
| Usar geração local e a API de completions | [Terminal](USO.md) e [HTTP](HTTP.md) |

Este repositório reúne documentação de uso, binários e interfaces públicas.
Os arquivos existentes do kit SDK têm suas próprias versões; para instalar
a central 0.10.0, use o APT conforme abaixo. Nenhum checkout privado é necessário.

## Instalação e atualização pelo APT (Debian 13 / amd64)

Configure o repositório uma vez. Como root, omita `sudo`:

```sh
sudo apt update
sudo apt install ca-certificates curl
curl -fsSLo /tmp/schip-apt-setup.sh https://deb.schip.com.br/setup.sh
sudo sh /tmp/schip-apt-setup.sh
```

Para instalar ou atualizar este projeto:

```sh
sudo apt update
sudo apt install schipia
```

O APT resolve as dependências. Pacotes `-dev` são opcionais e destinam-se à compilação.
Leia [Instalação](INSTALACAO.md) para configurar o sistema e resolver problemas.

## Outros recursos de utilização

- [Modelos](MODELOS.md): importar e escolher modelos para geração local.
- [Ferramentas](FERRAMENTAS.md): comandos disponíveis e verificação da instalação.
- [Desempenho](DESEMPENHO.md): modos de execução e medições.
- [Solução de problemas](SOLUCAO_DE_PROBLEMAS.md): diagnóstico de erros comuns.
- [Licenciamento](LICENCIAMENTO.md): condições de uso e licenças dos componentes.

As bibliotecas instaladas ficam em `/usr/lib/schip`. Os arquivos de implementação
da engine não são publicados aqui. A integração da central é feita pela API;
consulte os guias acima para os demais modos de utilização.
