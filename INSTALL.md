# schipia: distribuição binária

Versão **0.14.1**. Este repositório contém os pacotes, suas dependências Schip,
headers, documentação e exemplos. Nenhum checkout privado é necessário.

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
Leia [Configuração do APT](APT.md) para conferir o sistema, configurar manualmente,
resolver erros e atualizar serviços que compartilham bancos.

## Instalação com os arquivos deste kit

```sh
sha256sum -c SHA256SUMS
./install.sh
```

O APT instala as dependências do sistema. Os pacotes são compilados para a
arquitetura indicada em `release.json`; as versões mínimas de libc e demais
bibliotecas constam nos campos `Depends` dos `.deb`. Confira antes de instalar:

```sh
for p in debs/*.deb; do dpkg-deb -f "$p" Package Version Architecture Depends; done
```

As bibliotecas ficam em `/usr/lib/schip`; os headers dos pacotes `-dev`, em
`/usr/include/schip`. As aplicações dependem desses pacotes compartilhados.
Serviços seguem os scripts de instalação dos respectivos pacotes.

## Desenvolver usando somente este repositório

Instale `build-essential`, `make`, `python3`, `pkg-config`, `libssl-dev` e
`zlib1g-dev`. Os exemplos de clientes SQL também usam `libmariadb-dev`.

```sh
make             # compila os exemplos com include/ e lib/ deste kit
make check       # executa os exemplos locais de verificação
make clean       # limpa somente os builds e dados de demonstração
```

Não é necessário instalar os pacotes para compilar os exemplos. Clientes HTTP,
SMTP e servidores de demonstração são compilados, mas não iniciados pelo teste
automático. `make check` do servidor SQL usa um banco temporário e porta própria.

Para desenvolver contra os pacotes instalados:

```sh
./install.sh --dev  # inclui headers e dependências de compilação
cc app.c -o app $(pkg-config --cflags --libs libschipbase)
# Para o motor de banco, use libschip no lugar de libschipbase.
```

## Conteúdo e atualização

- `debs/`: pacotes prontos e índices APT, com todas as dependências Schip deste kit.
- `include/` e `lib/`: SDK da mesma versão dos pacotes, para os exemplos locais.
- `examples/` ou `exemplos/`: código de aplicação demonstrativo; a implementação das bibliotecas não é distribuída.
- `release.json` e `SHA256SUMS`: versões e integridade dos arquivos entregues.
- `REFERENCE.md`, `manual/` e `docs/`, quando presentes: referência de API e operação.

Bibliotecas e aplicações com ABI incompatível devem ser atualizadas juntas.
Recompile seus programas quando mudar o ABI. Reinicie processos que usam uma
biblioteca atualizada; um processo aberto pode continuar usando a versão anterior.

Os `.deb` já estão prontos: `make` aqui compila os exemplos, não a implementação
das bibliotecas ou os pacotes. Respeite os termos do arquivo `LICENSE`.
