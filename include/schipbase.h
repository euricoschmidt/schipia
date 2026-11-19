/*
 * libschipbase
 *
 * Copyright (c) 1988 Eurico Schmidt <euricoalvesschmidt@gmail.com>
 * All rights reserved.
 *
 * PROPRIETARY AND CONFIDENTIAL:
 * This software and associated documentation are the proprietary property of
 * Eurico Schmidt. Unauthorized copying, reverse engineering, decompilation,
 * distribution, or public display of this file or compiled binaries, via any
 * medium, is strictly prohibited without prior written permission.
 *
 * Provided under the terms of the Evaluation & Commercial License.
 * See LICENSE in the repository root for details.
 */

/**
 * @file schipbase.h
 * @brief Runtime base do ecossistema Schip: buffer dinamico, helpers de string e log.
 *
 * Header autocontido: usa apenas tipos nativos do C e a libc. Nao depende de
 * nenhum outro header do projeto, o que permite usar a lib isoladamente.
 *
 * Os nomes de struct (Tstr, Tbuf) sao preservados por compatibilidade com o
 * codigo existente, mas todos os campos e prototipos usam tipos nativos.
 */
#ifndef	schipbase_h
#define	schipbase_h		1

#include <stdarg.h>
#include <stddef.h>
#include <string.h>	/* memcpy: usado pelo caminho rapido inline Tbuf_put() */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	Ok
#define	Ok		0
#endif
#ifndef	False
#define	False		0
#endif
#ifndef	True
#define	True		1
#endif
#ifndef	ERRO
#define	ERRO		(-1)
#endif

#define	SCHIPBASE_VERSION_MAJOR	1
#define	SCHIPBASE_VERSION_MINOR	0
#define	SCHIPBASE_VERSION_PATCH	0

/** Tamanho de bloco usado nas leituras/escritas de arquivo. */
#define	SCHIP_BLOCK_SIZE	4096
/** Tamanho maximo do texto formatado nos helpers que recebem `fmt, ...`. */
#define	SCHIP_MAX_ARG		1024

/* ------------------------------------------------------------------ */
/* Memoria                                                             */
/* ------------------------------------------------------------------ */

/**
 * Aloca e zera `nelem * size` bytes, opcionalmente copiando `src` por cima.
 *
 * Diferente do antigo Tdb_memory, **nunca chama exit()**: uma biblioteca nao
 * derruba o processo do chamador. Retorna NULL em falha ou quando o produto
 * `nelem * size` transborda.
 */
void	*Tmem_alloc(size_t nelem, size_t size, const void *src);

/** Libera `*p` (se nao-NULL) e zera o ponteiro. Aceita `p` NULL. */
void	Tmem_free(void **p);

/* ------------------------------------------------------------------ */
/* Log                                                                 */
/* ------------------------------------------------------------------ */

/**
 * Assinatura do handler de log. Recebe `va_list` ja iniciado — assim o
 * repasse de varargs funciona sem truques de plataforma.
 */
typedef int (*Tschip_logfn)(const char *file, const char *func, int line,
			    const char *fmt, va_list ap);

/**
 * Handler ativo. Por padrao escreve uma linha em stderr.
 *
 * Aplicacoes que ja tem logger proprio (o `Tlog_schip` da libschip, por
 * exemplo) atribuem seu adaptador aqui e todo o log das libs Schip passa a
 * sair pelo canal delas.
 */
extern	Tschip_logfn	Tschip_log_handler;

/** Emite uma entrada de log pelo handler ativo. Retorna sempre ERRO, para
 *  permitir `return Tschip_log(...)` em caminhos de erro. */
int	Tschip_log(const char *file, const char *func, int line, const char *fmt, ...)
		__attribute__((format(printf, 4, 5)));

/** Handler padrao (stderr). Exposto para quem quiser encadear. */
int	Tschip_log_stderr(const char *file, const char *func, int line,
			  const char *fmt, va_list ap);

#define	Tschip_logw(...)	Tschip_log(__FILE__, (const char *) __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__)

/* ------------------------------------------------------------------ */
/* Strings de comprimento explicito                                    */
/* ------------------------------------------------------------------ */

/** String com comprimento explicito: nao exige terminador NUL. */
typedef	struct {
	char	*d,	/**< Inicio dos dados. */
		*e;	/**< Ultimo byte valido. */
	int	l;	/**< Comprimento em bytes. */
	} Tstr;

/* ------------------------------------------------------------------ */
/* Tabelas de opcao                                                    */
/* ------------------------------------------------------------------ */

/**
 * Entrada de uma tabela de opcoes (enum com rotulo).
 *
 * Um vetor de Toptm mapeia um codigo numerico para um ou dois rotulos, e deve
 * ser terminado por `{ NULL }`.
 */
typedef	struct {
	char	*s1;	/**< Rotulo principal. */
	int	idx;	/**< Codigo associado. */
	char	*s2;	/**< Rotulo secundario ou descricao (opcional). */
	} Toptm;

/** Busca por `idx` em vez de por posicao. */
#define	Toptm_flg_idx	0x80000000
/** Usa o rotulo secundario (`s2`). */
#define	Toptm_flg_s2	0x40000000
/** Faz Toptm_search() devolver `idx` em vez da posicao. */
#define	Toptm_get_idx	0x20000000
#define	Toptm_flg_mask	0x1fffffff

/** Rotulo da opcao em `pos` (posicao, ou codigo com Toptm_flg_idx). */
char	*Toptm_getstring(Toptm *m, int pos);
/**
 * Procura `s` na tabela. Devolve a posicao (ou `idx` com Toptm_get_idx), ou
 * ERRO. Com `s` NULL devolve o comprimento do maior rotulo; com `s` vazio,
 * o numero de entradas.
 */
int	Toptm_search(Toptm *m, const char *s, int pos);

/* ------------------------------------------------------------------ */
/* Buffer dinamico                                                     */
/* ------------------------------------------------------------------ */

/**
 * Buffer que cresce em multiplos de `salto`.
 *
 * Quando `pnt` e' nao-NULL o buffer opera em modo *passagem direta*: cada
 * Tbuf_write() delega para o callback `write` (por padrao o write(2) do
 * sistema) em vez de acumular em memoria.
 */
typedef	struct {
	Tstr	s;		/**< Dados acumulados. */
	void	*pnt;		/**< Destino do modo passagem direta (NULL = acumula em memoria). */
	int	maxsize,	/**< Capacidade alocada. */
		salto,		/**< Granularidade de crescimento (default 64). SO' isso:
				     o macro Tbuf_sort() ja' usou este campo como tamanho
				     de registro, e hoje recebe `recsize` explicito. Quem
				     ajusta o crescimento (schipmyserverd poe 4096 aqui)
				     nao pode estar mexendo em outra coisa junto. */
		pos,		/**< Cursor de leitura usado por Tbuf_read(). */
		(*write)(void *, const void *, int); /**< Callback do modo passagem direta. */
	int64_t	nitens;		/**< Numero de escritas efetuadas. */
	} Tbuf;

/** Cria um buffer com granularidade `salto` (0 = 64 bytes). NULL em falha. */
Tbuf	*Tbuf_new(int salto);
/** Libera o buffer e seus dados. Aceita NULL. */
void	Tbuf_close(Tbuf *b);
/**
 * Tbuf_new() de uma reserva por thread: o buffer volta vazio, em modo de
 * acumulacao e com a granularidade `salto`, mas reaproveita a memoria de um
 * uso anterior -- sem calloc/free. Para buffer de vida curta chamado com
 * frequencia (por linha, por comparacao). Devolva com Tbuf_give(), nunca
 * Tbuf_close(); pode ser chamado de novo antes de devolver (reentrante).
 */
Tbuf	*Tbuf_take(int salto);
/** Devolve a' reserva da thread um buffer de Tbuf_take() (ou Tbuf_new()); acima do limite, libera. Aceita NULL. */
void	Tbuf_give(Tbuf *b);
/** Zera conteudo e cursores; `salto` != 0 redefine a granularidade. */
void	*Tbuf_reset(Tbuf *b, int salto);
/** Anexa `size` bytes; `data` NULL anexa zeros. Retorna bytes escritos ou ERRO. */
int	Tbuf_write(Tbuf *b, const void *data, int size);
/**
 * Caminho rapido de Tbuf_write(): anexa `size` bytes quando o buffer acumula
 * em memoria e JA' TEM capacidade. Qualquer outro caso -- modo passagem
 * direta (`pnt`), `data` NULL, tamanho invalido, precisar crescer -- cai em
 * Tbuf_write(), que continua sendo a unica implementacao dessas situacoes.
 *
 * Existe porque Tbuf_write() e' um simbolo exportado de libschipbase.so:
 * mesmo com LTO, toda chamada sai pelo PLT e nao pode ser inlineada, e o
 * `memcpy` de 1 ou 4 bytes vira chamada de funcao. Os protocolos escrevem
 * campo a campo, entao isso e' por coluna, por linha. Medido no subspace
 * (i7-9700): 12,41 -> 3,98 ns para os tres campos de uma linha tipica, e
 * 4,33 -> 1,51 ns num unico byte.
 *
 * O corpo espelha EXATAMENTE o ramo de memoria de Tbuf_write(), inclusive
 * `s.e` apontando para o inicio do trecho recem-anexado e o NUL depois dele.
 * Quem mudar um lado muda os dois.
 */
static inline int	Tbuf_put(Tbuf *b, const void *data, int size) {
	if(b != NULL && b->pnt == NULL && data != NULL &&
	   size > 0 && size <= b->maxsize - b->s.l) {
		char	*e = b->s.d + b->s.l;

		memcpy(e, data, (size_t) size);
		b->s.l += size;
		e[size] = 0;
		b->s.e = e;
		b->nitens++;
		return size;
	}
	return Tbuf_write(b, data, size);
}
/** Le a partir do cursor interno. Retorna bytes lidos. */
int	Tbuf_read(Tbuf *b, void *data, int size);
/** Copia o conteudo para `buf` e termina em NUL. Retorna o comprimento. */
int	Tbuf_2char(char *buf, Tbuf *b);
/** Remove `size` bytes a partir de `pos`. */
void	*Tbuf_delete(Tbuf *b, int pos, int size);
/** Insere `size` bytes na posicao `pos`. */
void	*Tbuf_insert(Tbuf *b, int pos, const void *data, int size);
/** Anexa um byte. */
void	Tbuf_putc(Tbuf *b, unsigned char c);
/** Anexa texto formatado; `flg` != 0 zera o buffer antes. `b` NULL escreve em stdout. */
int	Tbuf_printf(Tbuf *b, short flg, const char *fmt, ...)
		__attribute__((format(printf, 3, 4)));
/** Grava o conteudo do buffer no arquivo cujo caminho e' formatado por `fmt`. */
int	Tbuf_putfile(Tbuf *b, const char *fmt, ...)
		__attribute__((format(printf, 2, 3)));
/** Carrega o arquivo inteiro no buffer, reservando `offset` bytes zerados ao fim. */
int	Tbuf_getfile(Tbuf *b, int offset, const char *fmt, ...)
		__attribute__((format(printf, 3, 4)));

#ifndef Tbuf_log
#define	Tbuf_log(b)	if((b)->s.l > 0) Tschip_logw("%d[%.*s]", (b)->s.l, (b)->s.l, (b)->s.d)
#endif

/* ------------------------------------------------------------------ */
/* Helpers de string                                                   */
/* ------------------------------------------------------------------ */

/** Remove brancos/pontos a esquerda e brancos a direita. Retorna o novo comprimento. */
int	Tchar_check(char *buf, int len);
/** Copia com preenchimento: `ch` != 0 preenche a sobra, senao aplica Tchar_check(). */
int	movestr(int size_d, void *dst, int size_s, const void *src, int ch);
/** Copia `src` em `dst` descartando brancos a direita e terminando em NUL. */
char	*Tchar_from_Tstring(char *dst, int size_dst, const char *src, int size_src);
/** Copia respeitando o NUL de origem e zerando o restante de `dst`. */
char	*Tchar_from_Tchar(char *dst, int sized, const char *src);
/** Copia `src` em `dst` preenchendo com espacos ate `size_dst` (sem NUL). */
char	*Tstring_from_Tstring(char *dst, int size_dst, const char *src, int size_src);
/** Copia uma Tstr para um buffer C terminado em NUL. */
char	*Tstr_char(const Tstr *s, char *buf, int maxsize);

/** Indice de `chave` em `cadeia` a partir de `inicio`, ou ERRO. */
int	procura(const char *cadeia, const char *chave, int inicio);
/** Desloca `size` bytes de `buf` para a direita em `offset`. */
char	*memshiftr(char *buf, int size, int offset);
/** Remove `del` e/ou insere `ins` em `buf`. Retorna o ponto apos a insercao. */
char	*strchange(char *buf, int buflen, const char *del, int dellen, const char *ins, int inslen);
/** Insere `n` na posicao `pos` de `b`. Retorna o novo comprimento. */
int	stradd(char *b, int bsize, const char *n, int nsize, int pos);
/** Remove `size` bytes a partir de `inicio`. Retorna `inicio`. */
int	strsub(char *buffer, int inicio, int size);
/** Remove ate `flg`+1 ocorrencias de `str`. */
int	strsubstr(char *buffer, const char *str, short flg);
/**
 * Filtra ou decodifica `url` em `p`.
 *
 * `flg` falso: copia apenas os digitos (util para extrair um numero de um
 * texto formatado). `flg` verdadeiro: decodifica percent-encoding (`%XX`) e
 * traduz `+` para espaco, como em query string de URL.
 *
 * Retorna o comprimento escrito. `p` pode ser igual a `url` (decodifica no
 * lugar, ja que a saida nunca e maior que a entrada).
 */
int	Tchar_strip(const char *url, char *p, short flg);

/** Resolve o numero de trabalhadores (workers) a partir de flag -w ou variavel de ambiente. */
int	Tschip_workers_config(int argc, char **argv, const char *envvar, int default_n, int max_n);

/* ------------------------------------------------------------------ */
/* Base64                                                              */
/* ------------------------------------------------------------------ */

#define	Tbase64_flg_url		0x01	/**< Alfabeto url-safe: `-_` em vez de `+/`. */
#define	Tbase64_flg_nopad	0x02	/**< Nao escreve o `=` de padding (JWT). */

/** Bytes necessarios em `dst` para codificar `n` bytes (inclui o NUL). */
#define	Tbase64_encsize(n)	(((n) + 2) / 3 * 4 + 1)
/** Bytes necessarios em `dst` para decodificar `n` caracteres (inclui o NUL). */
#define	Tbase64_decsize(n)	((n) / 4 * 3 + 1)

/**
 * Codifica `srclen` bytes de `src` em base64, sem quebra de linha.
 *
 * Escreve no maximo `dstsize` bytes, NUL incluido, e devolve o comprimento
 * escrito (sem contar o NUL) ou @ref ERRO se `dst` for pequeno demais --
 * dimensione com @ref Tbase64_encsize. `flg` aceita @ref Tbase64_flg_url e
 * @ref Tbase64_flg_nopad, combinaveis.
 */
int	Tbase64_encode(char *dst, int dstsize, const void *src, int srclen, short flg);

/**
 * Decodifica a string base64 `src` em `dst`.
 *
 * Devolve o numero de bytes escritos -- o comprimento REAL, ja' descontado o
 * padding -- ou @ref ERRO se nao couber em `dstsize`. Dimensione com
 * @ref Tbase64_decsize. Espacos, quebras de linha e lixo sao ignorados, e a
 * leitura para no primeiro `=`.
 *
 * Os dois alfabetos sao aceitos ao mesmo tempo (`+/` e `-_`), entao `flg` nao
 * e' usado hoje: uma string url-safe decodifica sem que o chamador precise
 * dizer. O parametro existe para nao mudar a assinatura se um dia for preciso
 * recusar um dos alfabetos.
 *
 * Se sobrar espaco, escreve um NUL depois do ultimo byte por conveniencia de
 * quem decodifica texto; o NUL nao entra na contagem devolvida.
 */
int	Tbase64_decode(void *dst, int dstsize, const char *src, short flg);

#ifdef __cplusplus
}
#endif

#endif
