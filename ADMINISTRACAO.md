# Administrar uma central de ajuda SchipIA 0.10.0

Para consumir a central hospedada em https://schipia.schip.com.br, basta uma
chave de integração e os passos de [API.md](API.md). Este guia é para quem
administra o servidor ou instala uma central própria.

## Instalação e primeira configuração

Instale `schipia=0.10.0` pelo APT conforme [INSTALACAO.md](INSTALACAO.md).
O pacote cria o usuário de serviço `schipia` e prepara a base inicial e o
vocabulário em `/var/lib/schipia-helpdesk/`. As respostas e o aprendizado
persistente ficam nesse servidor, compartilhados pelas integrações autorizadas.

O serviço exige `/etc/schipia/clients.tsv`. O exemplo instalado está em
`/usr/share/schipia/helpdesk/CLIENTES.example.tsv`. Cadastre uma linha por
integração, com sete campos separados por **TAB**, sem cabeçalho ativo:

```text
identificador<TAB>chave_hex_64<TAB>aplicativo<TAB>versoes_csv<TAB>perfil<TAB>organizacao<TAB>curador
```

`<TAB>` representa o separador, não texto literal. Exemplo de valores:
`app-operador`, uma chave gerada, `schipsave`, `0.40.3`, `operator`,
`empresa-a`, `0`. Os perfis da base inicial são `viewer`, `operator` e `admin`;
as versões são uma lista separada por vírgulas. Use `1` no último campo
somente para uma chave dedicada à curadoria. Separe organizações por chave.

Gere cada chave com `schipia-helpdesk keygen`, capturando a saída diretamente
em arquivo privado. O comando gera 64 caracteres hexadecimais. Não coloque
chaves em commits, exemplos públicos ou logs.

```sh
sudo chown schipia:schipia /etc/schipia/clients.tsv
sudo chmod 600 /etc/schipia/clients.tsv
sudo systemctl enable --now schipia-helpdesk
sudo systemctl status schipia-helpdesk
curl --fail http://127.0.0.1:8090/health
```

Após alterar o cadastro, reinicie `schipia-helpdesk`. No subspace, as chaves
estão nesse caminho e o inventário sem segredos está em
`/etc/schipia/profiles.tsv`. A distribuição das chaves aos backends é feita
pelo administrador.

## Endereço público

O serviço padrão escuta em `127.0.0.1:8090`. Configure seu proxy HTTPS para
encaminhar as requisições e o cabeçalho `Authorization` a esse endereço,
com limite de corpo de 32 KiB e timeout suficiente para a inferência.
No subspace, Nginx e o certificado já atendem `schipia.schip.com.br`.

O modelo de instruções e o vocabulário estão em
`/usr/share/schipia/models/helpdesk160m/`; o serviço usa uma cópia gravável do
vocabulário no diretório de estado. Para executar em primeiro plano:

```sh
sudo -u schipia schipia-helpdesk serve \
  /var/lib/schipia-helpdesk/knowledge /etc/schipia/clients.tsv \
  /var/lib/schipia-helpdesk/memory 8090 4 \
  /usr/share/schipia/models/helpdesk160m/model.sct_model \
  /var/lib/schipia-helpdesk/vocab.sct_tokens
```

Pare a unidade antes de executar esse comando: a memória admite um gravador
por vez. Os últimos dois caminhos são opcionais; omita ambos para atendimento
somente documental. O serviço instalado usa o modelo de instruções por padrão.

## Ativar os modelos opcionais

Siga [Modelos](MODELOS.md) para instalar Qwen3, os perfis de programação e o
modelo de busca, gerar o índice semântico e definir `SCHIPIA_MODELS` e
`SCHIPIA_SEMANTIC` no ambiente da instância. A conta do serviço deve ler os
modelos e o índice e gravar apenas em seu próprio diretório de estado.

Configure cliente e proxy com timeout de pelo menos 180 segundos como ponto
de partida para geração em CPU. Um proxy de 25 segundos pode interromper a
resposta antes de ela terminar. Verifique `GET /v1/models` autenticado antes
de habilitar a programação no aplicativo.

No subspace, mantenha `schipia-helpdesk.service` como central existente e
`schipia-lab-010.service` como laboratório. O laboratório escuta somente em
`127.0.0.1:8091`, usa `/var/lib/schipia-lab-010` para seu estado e mantém os
arquivos em `/srv/schipia-lab/0.10.0`. Não exponha essa porta publicamente nem
compartilhe o diretório de memória com a central. A publicação pelo APT
distribui o pacote; não promove o laboratório para produção.

Após atualizar os manuais ou o modelo de busca, regenere o índice semântico
da instância. Faça backup e pare a instância antes de alterar sua base ativa.

## Importar e atualizar manuais

Crie um manifesto TSV UTF-8 com sete colunas: `id`, `application`, `version`,
`screen`, `roles`, `file`, `url`. Linhas iniciadas por `#` são comentários.
`roles` aceita uma lista separada por vírgulas; `file` é o caminho de um manual
Markdown, relativo ao diretório de execução ou absoluto. Use IDs estáveis.

Exemplo, substituindo `<TAB>` pelo separador real:

```text
meuapp-uso<TAB>meuapp<TAB>1.0.0<TAB>cadastros<TAB>operator,admin<TAB>manuais/uso.md<TAB>https://exemplo.com/manual
```

Importe para um diretório novo:

```sh
schipia-helpdesk ingest manifesto.tsv /caminho/privado/knowledge-nova
```

Antes de ativar a base, teste perguntas e recusas para os perfis atendidos.
Pare o serviço, faça backup e substitua
`/var/lib/schipia-helpdesk/knowledge` pela nova base, com proprietário
`schipia:schipia`. Preserve o diretório `memory` e reinicie a unidade.
Cadastre também as novas versões nas chaves autorizadas.

O pacote preserva uma base já existente: instalar uma atualização não
substitui seus manuais personalizados. Avaliações antigas deixam de promover
um trecho quando seu texto documental muda. Contribuições aprovadas permanecem
na memória, vinculadas à versão/contexto em que foram cadastradas.

## Retenção, backup e verificação

Faça backup de **todo** `/var/lib/schipia-helpdesk/` com o serviço parado,
e preserve separadamente as credenciais e a versão do pacote. Restrinja os
backups: a memória contém perguntas, respostas e contribuições dos usuários.

A capacidade desta versão é de 65.536 eventos por diretório de memória.
Ao atingir o limite, consultas documentais continuam com `retained=false`, mas
novas contribuições precisam de administração do armazenamento. Acompanhe esse
campo e o espaço em disco. Preserve o diretório completo antes de arquivar ou
iniciar uma nova memória; não apague arquivos isolados do estado.

```sh
sudo journalctl -u schipia-helpdesk -n 30 --no-pager
sudo /usr/libexec/schipia/test_helpdesk.sh
```

O segundo comando testa o modelo, as perguntas documentais, permissões e
aprendizado em memórias temporárias. Para a verificação completa do pacote,
use `/usr/libexec/schipia/run_checks.sh --valgrind --benchmarks` com Valgrind
instalado. São testes que consomem CPU; execute em uma janela apropriada.
