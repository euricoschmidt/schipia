# Escolha e conversão de modelos

Os exemplos pressupõem `MODEL` e `STATE` definidos em [Instalação](INSTALACAO.md).
Use diretórios novos para conversões e contextos diferentes para cada modelo.
Nenhuma conversão exige baixar novamente o checkpoint fornecido no pacote.

## Qual formato escolher

| Formato | Quando usar | Tamanho aproximado do Tucano fornecido |
|---|---|---|
| BF16 | Padrão distribuído; preserva os pesos de origem. | 325 MB |
| Q8 | Menos memória e maior velocidade, com pequena alteração numérica. | 173 MB |
| Q4 | Priorizar tamanho e velocidade; verificar a qualidade do texto. | 92 MB |

Tamanho do arquivo não é o consumo total de memória: o programa também precisa
de espaço para o contexto. Q8 e Q4 alteram os pesos. Resultados de um formato
não devem ser tratados como idênticos aos de outro.

## Criar Q8 ou Q4

```sh
mkdir -p "$STATE/modelos/q8" "$STATE/modelos/q4"
schipia-pack --q8 "$MODEL/model.sct_model" "$STATE/modelos/q8/model.sct_model"
cp -r "$MODEL/vocab.sct_tokens" "$STATE/modelos/q8/"
schipia-pack --q4 "$MODEL/model.sct_model" "$STATE/modelos/q4/model.sct_model"
cp -r "$MODEL/vocab.sct_tokens" "$STATE/modelos/q4/"
```

Para usar o Q4, passe `"$STATE/modelos/q4/model.sct_model"` e
`"$STATE/modelos/q4/vocab.sct_tokens"` ao `engine_run`, com um contexto novo.
O mesmo vale para Q8. Prefira partir do BF16 original em vez de quantizar
novamente um modelo que já sofreu redução de precisão.

Sem opção de formato, `schipia-pack ORIGEM DESTINO` tenta converter para BF16
sem perder informação. Se os pesos não puderem ser representados exatamente,
a operação falha. Um modelo que já está em BF16 não precisa dessa conversão.

## Bundle em árvore e alinhamento de páginas

Requer executáveis com `--tree` na ajuda de `schipia-pack`. Para criar um bundle
Q4, primeiro faça a conversão Q4 acima e depois empacote em árvore:

```sh
schipia-pack --tree --page-align "$STATE/modelos/q4/model.sct_model" \
  "$STATE/modelos/q4-tree"
cp -r "$MODEL/vocab.sct_tokens" "$STATE/modelos/q4-tree/"
```

`--q4`, `--q8` e `--tree` são alternativas: não as combine na mesma chamada.
`--page-align` é usado com `--tree`. O diretório resultante precisa ser mantido
inteiro, incluindo todos os arquivos `weights.*`; não copie apenas
`model.sct_model`. Para carregar o bundle e seu índice, passe o diretório como
argumento de modelo. Use um destino novo para evitar sobrescrever dados existentes.
O procedimento documentado e validado aqui usa Q4; mantenha o BF16 no formato
padrão, sem aplicar esse procedimento de alinhamento a ele.

O bundle é utilizado pelo mesmo comando de geração:

```sh
engine_run generate "$STATE/modelos/q4-tree" \
  "$STATE/modelos/q4-tree/vocab.sct_tokens" "$STATE/historia-tree" 1 \
  --new --prompt 'Era uma vez um pássaro azul' --tokens 64 --threads 4
```

## Esparsidade experimental

Se a ajuda de `engine_run` listar `--sparse`, você pode acrescentar
`--sparse 0.3` à geração Q4/Q8. Comece comparando com `--sparse 0`, em contextos
separados. Mantenha o mesmo limiar durante toda a utilização de cada contexto.

Um limiar positivo pode mudar a saída e piorar a qualidade; economizar acessos
não garante reduzir o tempo. O valor 0 é o padrão e desativa esse comportamento.
Escolha a configuração depois de medir sua própria tarefa. Não há promessa de
ganho universal para o modelo fornecido.

## Importar checkpoints compatíveis

Para o perfil auditado Tucano-160M, os três arquivos de origem devem pertencer à
mesma revisão: `6331cbef9933c05d8d0c8ae5ee23ed2570f06571` de
[TucanoBR/Tucano-160m](https://huggingface.co/TucanoBR/Tucano-160m).

```sh
schipia-import-tucano model.safetensors config.json tokenizer.json native-pt
```

A saída `native-pt` deve ser nova. O importador não aceita qualquer modelo de
Hugging Face, nem adapta automaticamente tokenizadores diferentes. Reserve
espaço para os arquivos de origem, temporários e modelo convertido. Depois, a
conversão para BF16 usa `schipia-pack`, mantendo o vocabulário correspondente.

Também existe `schipia-import-llama CHECKPOINT.bin TOKENIZER.bin DIRETORIO` para
checkpoints FP32 originais llama2.c compatíveis. Não é um leitor de GGUF. O
modelo e o tokenizador precisam corresponder; trocar apenas um deles não é
uma forma válida de alterar o idioma ou a capacidade do modelo.
