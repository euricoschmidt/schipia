# Instalação e atualização

A versão atual no repositório APT é **0.10.0**, para **Debian 13 (trixie), amd64**.
Ela inclui a central de ajuda, seus manuais iniciais e o modelo de instruções
em português. Não é preciso instalar Python ou PyTorch.

Os novos Qwen de diálogo, programação e busca são opcionais e baixados
separadamente com `schipia-models`. Consulte [Modelos](MODELOS.md) para instalar
e ativar esses perfis. Atualizar o pacote preserva o estado e o atendimento
existente; não ativa Qwen automaticamente. No subspace, esses perfis continuam
na instância de laboratório, separada da central pública.

Para usar a central já hospedada, acesse https://schipia.schip.com.br com sua
chave autorizada, ou integre o aplicativo conforme [API.md](API.md).
O aplicativo cliente não precisa instalar os modelos localmente.

Para instalar ou atualizar uma central própria, configure o APT conforme a
seção abaixo e execute `sudo apt update` e `sudo apt install schipia=0.10.0`.
Confira com `dpkg-query -W schipia`. A instalação fornece a unidade
`schipia-helpdesk.service`; ela precisa de um cadastro de clientes antes de
iniciar. Siga [Administração](ADMINISTRACAO.md) para configurar as credenciais.
Atualizações preservam a memória, as chaves e uma base de manuais já existente.

Os arquivos de execução direta e SDK presentes em `bin/`, `lib/` e `include/`
pertencem ao kit existente, com versões próprias. As instruções a seguir sobre
esses arquivos continuam disponíveis para esse modo de utilização.

## Execução direta a partir deste repositório (`bin/` + `lib/`)

Na raiz deste repositório, exporte `LD_LIBRARY_PATH` para carregar `libschipia.so`,
`libschiptree.so.1.0` e `libschipjson.so`:

```sh
export LD_LIBRARY_PATH="$PWD/lib:${LD_LIBRARY_PATH:-}"
./bin/engine_run demo /tmp/schipia-demo 42
./bin/test_kernels
./bin/bench_kernels
```

Para compilar uma aplicação C usando a biblioteca compartilhada:

```sh
cc minha_app.c -I./include -L./lib -Wl,-rpath,'$ORIGIN/lib' -lschipia -lschiptree -lm -fopenmp -o minha_app
```

## Configurar o repositório Schip (APT)

O repositório oficial é <https://deb.schip.com.br>. Configure-o uma vez:

```sh
sudo apt update
sudo apt install ca-certificates curl
curl -fsSLo /tmp/schip-apt-setup.sh https://deb.schip.com.br/setup.sh
sudo sh /tmp/schip-apt-setup.sh
```

O script configura o repositório; a instalação da aplicação é uma etapa separada.
Confira a disponibilidade do pacote:

```sh
apt-cache policy schipia
```

Quando a versão desejada estiver publicada nesse repositório:

```sh
sudo apt update
sudo apt install schipia
```

Uma versão já instalada pode aparecer como `Candidate` apenas por estar no
computador. Confira também a origem exibida: `/var/lib/dpkg/status` sozinho não
significa que essa versão está publicada no APT.

## Instalar um .deb recebido do fornecedor

Este repositório público contém documentação; solicite o pacote ao fornecedor.
Na pasta em que recebeu o arquivo:

```sh
dpkg-deb -f ./schipia_0.10.0_amd64.deb Package Version Architecture Depends
sudo apt install ./schipia_0.10.0_amd64.deb
engine_run --version
engine_run --help
```

O APT resolve as dependências disponíveis, incluindo bibliotecas Schip e OpenMP.
Se houver um arquivo de checksums fornecido com o pacote, confira-o antes da
instalação usando `sha256sum -c NOME_DO_ARQUIVO`.

## Preparar o uso sem root

Faça a instalação com privilégios administrativos e execute o programa com seu
usuário habitual. Copie o modelo para uma área sua, pois o vocabulário precisa
permitir escrita. Os exemplos abaixo serão usados nos demais guias:

```sh
mkdir -p "$HOME/.local/share/schipia" "$HOME/.local/state/schipia"
cp -r /usr/share/schipia/models/tucano160m "$HOME/.local/share/schipia/"
chmod -R u+rwX "$HOME/.local/share/schipia/tucano160m"
export MODEL="$HOME/.local/share/schipia/tucano160m"
export STATE="$HOME/.local/state/schipia"
```

Se já fez essa cópia, não repita o `cp` por cima dela. Após atualizar o pacote,
guarde uma cópia dos dados antigos e escolha uma pasta nova para o modelo novo.
Não reutilize contextos de outro modelo ou vocabulário.

Os comandos `export` valem para o terminal atual. Execute-os novamente ao abrir
outro terminal. O pacote também fornece ferramentas em `/usr/bin`, o modelo em
`/usr/share/schipia/models/tucano160m` e textos de licença em
`/usr/share/doc/schipia`.

## Atualizar e remover

Feche sessões interativas e pare o servidor HTTP antes de atualizar. Instale o
novo `.deb` com `apt install ./ARQUIVO.deb`, ou use o APT quando publicado.
Reinicie os programas após a atualização. A remoção usa `sudo apt remove schipia`;
os modelos copiados e os contextos em sua pasta pessoal continuam sendo seus
dados e devem ser administrados separadamente.

Próximo passo: [gerar e continuar histórias](USO.md).
