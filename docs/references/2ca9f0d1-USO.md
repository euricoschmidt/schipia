# Central de ajuda SchipIA 0.9.0

## Uma base para os aplicativos

A central reúne os manuais em um serviço no servidor da IA. Cada aplicativo
consulta `POST /v1/helpdesk/answer`, informando aplicativo, versão funcional,
tela e identificação do usuário. A chave da integração determina o perfil e
o cliente/organização. Não é possível mudar esses privilégios no corpo da pergunta.

A resposta traz o trecho do manual, sua identificação e um link para consultar
a fonte completa pela mesma API autenticada. Quando não há base suficiente,
a central informa que não encontrou a resposta, sem inventar um procedimento.

## Aprendizado com as conversas

Os diálogos ficam registrados no servidor. Uma avaliação positiva em
`POST /v1/helpdesk/feedback` registra a pergunta como uma maneira útil de encontrar
o procedimento citado. Esse registro ajuda a recuperar o mesmo procedimento em
perguntas futuras, sempre respeitando aplicativo, versão, perfil e organização.

Uma resposta negativa deixa de promover essa associação. Se o texto do manual
original mudou, a associação antiga não é aplicada automaticamente.

## Acrescentar informação nova

Envie uma pergunta, um título e a informação para
`POST /v1/helpdesk/contributions`. A contribuição é guardada imediatamente com
estado `pending`. Ela ainda não é usada nas respostas compartilhadas.

Um perfil autorizado consulta `POST /v1/helpdesk/pending`, confere a informação
no aplicativo e decide em `POST /v1/helpdesk/review`, com `approve` ou `reject`.
Após aprovação, a nova informação passa a ser consultável no mesmo contexto de
aplicativo, versão, organização e perfil para o qual foi contribuída. A rejeição
retira essa informação das respostas, preservando o registro da decisão.

## Retenção e atualização

A memória sobrevive à reinicialização do serviço. O aprendizado ocorre na base
de conhecimento e nas associações de perguntas; os pesos do modelo não são
retreinados automaticamente a cada diálogo. Não é preciso reinstalar o programa
para usar uma contribuição aprovada.

Guarde backups do diretório inteiro de memória com o serviço parado. A memória
cresce conforme o uso, até 65.536 eventos por diretório nesta versão. Ao atingir
o limite, as respostas documentais continuam disponíveis com `retained=false`;
novas contribuições precisam aguardar administração do armazenamento. Preserve
o diretório original e planeje seu arquivamento antes de iniciar uma nova memória.

## Modelos e respostas

O pacote inclui um Tucano-160M ajustado para instruções em português. O modelo
ajuda a selecionar entre trechos pertinentes; as instruções exibidas ao usuário
são retiradas da documentação autorizada, sem paráfrase livre do procedimento.
Se a sugestão do modelo não for válida, vale a seleção documental. Isso reduz
o risco de inventar comandos, telas ou permissões, mas não substitui a revisão
da qualidade e atualização dos próprios manuais.

## Aplicativos cobertos

Os manuais iniciais incluem uso de SchipSave, SchipFS e SchipIA. Os demais
aplicativos com guias de instalação disponíveis entram com esse conteúdo e a
tela `instalacao`, para perfil administrativo. Esse cadastro não significa que
todas as operações de todos os aplicativos já estejam documentadas. Novos
manuais e contribuições validadas ampliam a cobertura na mesma base central.
