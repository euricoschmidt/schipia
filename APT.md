# Configurar o APT e instalar pacotes Schip

Repositório: **https://deb.schip.com.br**. Os pacotes disponíveis são para
**Debian 13 (trixie), arquitetura amd64**. Não use esta fonte em outra versão
do Debian, no Ubuntu ou em outra arquitetura.

**Atualização para Schipmyserver/libschip 0.5.28:** leia as
[notas da versão atual](https://deb.schip.com.br/schipmyserver/0.5.28/RELEASE.md)
e o [inventário e procedimento de atualização](https://deb.schip.com.br/schipmyserver/0.5.27/UPGRADE.md).
Use o candidato atual do APT; não fixe versões dos exemplos históricos.
Os novos pacotes coordenam parada, backup frio conferido, migração offline
dos mapas v1, preservação da configuração e validação antes da retomada.
Planeje a janela e o quórum Fleet, confira espaço e consumidores adicionais.
Mapas v2 existentes são validados sem reconversão. A configuração existente
de Fleet é preservada; mantenha durabilidade ativa e experimentais desligados.

Os exemplos usam `sudo`. Se já estiver como **root**, execute os mesmos comandos
sem `sudo`; não é necessário instalar sudo para isso.

## 1. Conferir o sistema

```sh
cat /etc/os-release
dpkg --print-architecture
```

Confira `ID=debian`, `VERSION_CODENAME=trixie` e arquitetura `amd64`.

## 2. Configurar o repositório uma vez

```sh
sudo apt update
sudo apt install ca-certificates curl
curl -fsSLo /tmp/schip-apt-setup.sh https://deb.schip.com.br/setup.sh
sudo sh /tmp/schip-apt-setup.sh
```

O script verifica o sistema, baixa a chave pública, confere seu checksum e cria:

- `/etc/apt/keyrings/schip-archive-keyring.asc`: chave de assinatura dos pacotes.
- `/etc/apt/sources.list.d/schip.sources`: configuração da fonte APT.

Depois executa `apt-get update`. Ele não instala aplicações. Pode ser executado
novamente para restaurar a configuração da fonte e da chave.
Também instala a integração de serviços em `/usr/lib/schip/apt-services` e
`/etc/apt/apt.conf.d/80schip-services`. Em máquinas configuradas antes dessa
integração, execute novamente o script acima **antes do próximo upgrade**.

Confira se o APT encontrou os pacotes:

```sh
apt-cache policy schipmyserver
```

A saída deve mostrar uma versão candidata e a origem `https://deb.schip.com.br`.

## 3. Instalar

```sh
sudo apt install schipmyserver
```

Troque o nome pelo programa desejado. Exemplos: `trader-schip`, `schippay`,
`schipay-preview`, `vaiterbolo`, `schip-mcpserver` e `jvelha3d`.
`schippay` e `schipay-preview` são pacotes distintos; escolha o produto desejado.
O APT baixa e instala as bibliotecas necessárias automaticamente.
Não é necessário baixar cada `.deb` ou instalar pacotes `-dev` para executar.

## 4. Atualizar

```sh
sudo apt update
sudo apt install --only-upgrade schipmyserver
```

Isso atualiza o programa já instalado e resolve suas dependências. Repita com
o nome de outro programa para atualizá-lo. Para atualizar o sistema inteiro,
revise a lista apresentada por `sudo apt upgrade` antes de confirmar.
As aplicações podem reiniciar seus serviços durante a instalação; programe
a atualização para um horário em que essa interrupção seja aceitável.

### Quando vários serviços usam os mesmos bancos

Os pacotes atuais incluem o coordenador de manutenção mesmo quando os
hooks APT anteriores ainda estão instalados. Ele para os consumidores
identificados, faz backup frio verificado e valida os arquivos e mapas antes
de abrir o SQL. As aplicações retornam após a verificação SQL/Fleet.
Configurações e estados anteriores dos serviços são preservados. Consulte
o guia da versão para registrar raízes, instâncias e produtores adicionais.

Um processo fora de um serviço systemd precisa ser parado manualmente; nesse
caso, o APT interrompe a operação antes de substituir os arquivos e mostra o PID.
Se a instalação falhar, preserve os bloqueios e o backup. O estado atual fica
em `/var/lib/schip/package-maintenance/active.json`; hooks antigos também podem
manter `/var/lib/schip/apt-maintenance`. Não apague esses registros. Corrija a
causa e siga a recuperação do guia, incluindo configuração dos pacotes e
`sudo python3 /usr/lib/schip/schip-package-upgrade.py finalize`.

Prefira APT com o conjunto coerente indicado no guia. O preinst dos pacotes
novos também protege o uso direto de dpkg, mas o APT resolve dependências.
Não reinicie serviços manualmente para contornar uma validação recusada.

As bibliotecas podem já ter sido desempacotadas antes do bloqueio: processos
antigos precisam reiniciar para carregar o mesmo motor atualizado. A repetição
do comando configura também os pacotes pendentes. Não contorne essa proteção
com `SCHIPMY_ALLOW_SHARED_ENGINE=1`. Confira ao final `sudo dpkg --audit`
(saída vazia indica ausência de pendências) e o estado dos serviços com `systemctl`.

## Desenvolvimento

Para compilar programas que usam o motor de banco:

```sh
sudo apt install build-essential pkg-config libschip-dev
```

Os pacotes `-dev` trazem headers e dependências de desenvolvimento. As bibliotecas
ficam em `/usr/lib/schip`; os headers, em `/usr/include/schip`.
Consulte o README do projeto para exemplos e bibliotecas específicas.

## Configuração manual (alternativa ao script)

Após instalar `curl` e `ca-certificates`, execute:

```sh
curl -fsSLo /tmp/schip-archive-keyring.asc https://deb.schip.com.br/schip-archive-keyring.asc
sudo install -d -m 0755 /etc/apt/keyrings
sudo install -m 0644 /tmp/schip-archive-keyring.asc /etc/apt/keyrings/schip-archive-keyring.asc
sudo tee /etc/apt/sources.list.d/schip.sources >/dev/null <<'EOF'
Types: deb
URIs: https://deb.schip.com.br
Suites: trixie
Components: main
Architectures: amd64
Signed-By: /etc/apt/keyrings/schip-archive-keyring.asc
EOF
sudo apt update
```

Se tiver GnuPG instalado, confira a chave com
`gpg --show-keys --with-fingerprint /etc/apt/keyrings/schip-archive-keyring.asc`.
A impressão digital é `E5D6D296F1DC1DC41FC854060BD84F82256E033E`.
A opção `Signed-By` limita essa chave à fonte Schip.

## Problemas comuns

- **Pacote não encontrado ou sem versão candidata:** confira sistema e
  arquitetura, execute a configuração acima e verifique os erros de `apt update`.
- **Erro de assinatura ou chave ausente:** execute novamente o script de
  configuração. Não desative a verificação de assinatura.
- **Erro de HTTPS:** confira data/hora do sistema e instalação de `ca-certificates`.
- **Fonte duplicada ou conflito em Signed-By:** mantenha uma única configuração
  para `deb.schip.com.br`, em `schip.sources`; remova somente entradas duplicadas
  desse repositório em outros arquivos de `/etc/apt/sources.list.d/` ou `sources.list`.
- **Bibliotecas de desenvolvimento instaladas sem necessidade:** para novas
  instalações use APT pelo nome do programa ou `./install.sh` do kit. A opção
  `./install.sh --dev` é destinada a quem vai compilar.

As revisões `.rDATAHORA` identificam as publicações. Consulte
[versions.json](https://deb.schip.com.br/versions.json) para as versões disponíveis.

A publicação mantém as duas versões mais recentes de cada pacote por arquitetura.
Os repositórios de agentes também separam distribuição e versão do sistema.
A limpeza ocorre após a ativação verificada do novo snapshot assinado. Clientes
com índices antigos devem executar `apt update` antes de instalar ou atualizar.
