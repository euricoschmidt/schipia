/*
 * libschipjson
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
 * @file schipjson.h
 * @brief Parser e gerador de JSON e XML do ecossistema Schip.
 *
 * Header autocontido: depende apenas de <schipbase.h> (buffer e strings) e da
 * libc. Nenhum tipo do banco de dados aparece nesta interface.
 *
 * O parser opera **in-place** sobre o buffer: `Tstr.d`/`Tstr.l` apontam para
 * dentro do proprio texto de entrada, sem copia por token. Por isso o buffer
 * deve permanecer vivo enquanto os tokens forem usados.
 *
 * Uso tipico de leitura:
 * @code
 *   Tbuf *b = Tbuf_new(0);
 *   Tags *j = Tags_open(b, NULL, 0);
 *   Tjson_open(j, texto, 0);
 *   Tjson_forall(j) {
 *       // j->d.tag / j->d.tagdata / j->d.type
 *   }
 *   Tags_close(j);
 * @endcode
 */
#ifndef	schipjson_h
#define	schipjson_h		1

#include <schipbase.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	SCHIPJSON_VERSION_MAJOR	1
#define	SCHIPJSON_VERSION_MINOR	0
#define	SCHIPJSON_VERSION_PATCH	0

/* ------------------------------------------------------------------ */
/* Limites                                                             */
/* ------------------------------------------------------------------ */

#ifndef	MAX_TAGLEN
#define	MAX_TAGLEN		1024
#endif
#ifndef	MAX_TAGDATALEN
#define	MAX_TAGDATALEN		16384
#endif
#ifndef	MAX_NSLEN
#define	MAX_NSLEN		32
#endif

/** Profundidade maxima de aninhamento de objetos/arrays. */
#define	Tags_max_level		20
/** Passo de crescimento do vetor de tags coletadas. */
#define	Tags_array_block_size	10

/* ------------------------------------------------------------------ */
/* Formatos e tipos de token                                           */
/* ------------------------------------------------------------------ */

#define	Tags_json		0
#define	Tags_xml		1
#define	Tags_sql		2
#define	Tags_csv		3
#define	Tags_txt		4

#define	Tags_type_end		0
#define	Tags_type_close		1
#define	Tags_type_object	2
#define	Tags_type_array		3
#define	Tags_type_null		4
#define	Tags_type_string	5
#define	Tags_type_char		6
#define	Tags_type_boolean	7
#define	Tags_type_tiny		8
#define	Tags_type_short		9
#define	Tags_type_int		10
#define	Tags_type_long		11
#define	Tags_type_double	12
#define	Tags_type_hex		13
#define	Tags_type_rec		14
#define	Tags_type_time		15
#define	Tags_type_var		16
#define	Tags_type_memo		17
#define	Tags_type_max		18

/* Flags de construcao (Tags_new/Tags_open) */
#define	Xml_tabs		0x01	/**< Indenta a saida com tabs e quebras de linha. */
#define	Xml_type		0x02	/**< Documento XML (emite o prologo `<?xml ...?>`). */
#define	Html_type		0x08	/**< Documento HTML (emite `<!DOCTYPE html>`). */

#define	Tags_event_create	0x0001
#define	Tags_event_open		0x0002
#define	Tags_event_next		0x0004
#define	Tags_event_write	0x0008
#define	Tags_flg_once		0x0010	/**< Cada campo de Tpars e' preenchido uma unica vez. */
#define	Tags_flg_noalloc	0x0020	/**< O Tbuf pertence ao chamador; Tags_close() nao o libera. */
/**
 * Suprime o prologo XML/HTML. Tags_open() liga este bit automaticamente.
 *
 * No codigo original essa supressao era feita com Tags_event_open (0x0002),
 * que colide com Xml_type (0x02) — o efeito era que `Tags_new(b, f, Xml_type)`
 * nunca emitia `<?xml ...?>`. Os dois bits agora sao independentes.
 */
#define	Tags_flg_noheader	0x4000

/* Flags de dump (usadas pelas pontes da aplicacao) */
#define	Tags_flg_register	0x0001
#define	Tags_flg_login		0x0002
#define	Tags_flg_logout		0x0004
#define	Tags_flg_setup		0x0008
#define	Tags_flg_site		0x0010
#define	Tags_flg_field		0x0020
#define	Tags_flg_db		0x0040
#define	Tags_flg_ds		0x0080
#define	Tags_flg_sql		0x0100
#define	Tags_flg_pack		0x0200
#define	Tags_flg_rawdata	0x0400
#define	Tags_flg_dataset	0x0800
#define	Tags_flg_api		0x1000
#define	Tags_flg_websocket	0x2000

/**
 * Codigos passados ao callback `func`. Os valores coincidem com
 * Tflat_event_next/Tflat_event_write da SchipDB — nao altere sem revisar la.
 */
#define	Tjson_event_next	1
#define	Tjson_event_write	5

/* ------------------------------------------------------------------ */
/* Estruturas                                                          */
/* ------------------------------------------------------------------ */

typedef struct Tags Tags;

/** Token corrente do parser/gerador. */
typedef	struct {
	unsigned char	s,
			status;		/**< True quando o token ja tem chave associada. */
	char		*p;		/**< Cursor no texto de entrada. */
	int		flg,		/**< Flags de construcao herdadas do Tags. */
			nelem,		/**< Numero de elementos do nivel corrente. */
			pos;		/**< Indice do elemento dentro do nivel. */
	short		level,		/**< Nivel de aninhamento do token. */
			mlevel,		/**< Nivel maximo ja aberto. */
			type;		/**< Um dos Tags_type_*. */
	Tstr		tag,		/**< Nome da chave (aponta para o buffer). */
			tagdata;	/**< Valor (aponta para o buffer). */
	} Tjtag;

/** Estado especifico de XML: namespace, prologo e buffers de tag. */
typedef	struct {
	char	*cmpl,			/**< Atributos extras da proxima tag. */
		*vxml,			/**< Versao XML (default "1.0"). */
		*ns,			/**< Prefixo de namespace. */
		*charset,		/**< Charset do prologo. */
		xname[MAX_TAGLEN],	/**< Nome da tag corrente. */
		xdata[MAX_TAGDATALEN],	/**< Conteudo da tag corrente. */
		nsin[32];		/**< "<" + ns, pre-computado. */
	} Txml;

/** Pilha de nomes de tag abertos (usada para fechar na ordem certa). */
typedef	struct {
	char	ns[MAX_NSLEN],
		*data;
	} Tdata_d;

/** Contexto de documento. Criado por Tags_new()/Tags_open(). */
struct Tags {
	Txml	x;
	void	*h;		/**< Uso livre da aplicacao (hash de indice, etc). */
	Tbuf	*b;		/**< Buffer de entrada/saida. */
	int	maxtag,
		(*func)(Tags *, int);	/**< Callback de eventos (pode ser NULL). */
	short	ntag,
		versao;
	Tjtag	d,			/**< Token corrente. */
		*l,			/**< Nivel corrente na pilha `nj`. */
		*t,			/**< Vetor de tokens coletados. */
		nj[Tags_max_level];	/**< Pilha de niveis abertos. */
	Tdata_d	*n;
};

/**
 * Descritor de campo para leitura/escrita dirigida por tabela.
 *
 * Um vetor de Tpars mapeia nomes de chave JSON para enderecos em memoria;
 * Tjson_get() percorre o vetor e converte nos dois sentidos.
 */
typedef struct Tpars {
	char	x,		/**< Marcado quando o campo ja foi preenchido (com Tags_flg_once). */
		*s;		/**< Nome da chave. */
	short	type,		/**< Um dos Tags_type_*. */
		size;		/**< Tamanho do destino em bytes. Em Tags_type_double escolhe o tipo: sizeof(float), sizeof(double) ou (outro) long double. */
	void	*p;		/**< Endereco do destino. */
	short	(*func)(struct Tpars *, char *, short); /**< Conversor (NULL = Tjson_native). */
	} Tpars;

/* ------------------------------------------------------------------ */
/* Hooks de integracao                                                 */
/* ------------------------------------------------------------------ */

/**
 * Conversor para Tags_type_var. A lib nao conhece o tipo `Tvar` da SchipDB;
 * quem quiser suporte a esse tipo instala aqui um adaptador. Sem handler,
 * campos Tags_type_var sao tratados como string.
 */
typedef short (*Tjson_varfn)(void *dst, const char *src, int flg1, int flg2);
extern	Tjson_varfn	Tjson_var_handler;

/* ------------------------------------------------------------------ */
/* Ciclo de vida                                                       */
/* ------------------------------------------------------------------ */

/** Cria um contexto. `b` NULL aloca um Tbuf proprio. Retorna NULL em falha. */
Tags	*Tags_new(Tbuf *b, int (*func)(Tags *, int), int flg);
/** Como Tags_new(), mas sinaliza Tags_event_open ao callback. */
Tags	*Tags_open(Tbuf *b, int (*func)(Tags *, int), int flg);
/** Libera o contexto (e o Tbuf, se foi a lib que o alocou). */
void	Tags_close(Tags *j);
/** Zera o estado de parsing preservando buffer, callback e vetor de tokens. */
void	Tjson_reset(Tags *j);

/* ------------------------------------------------------------------ */
/* Leitura de JSON                                                     */
/* ------------------------------------------------------------------ */

/** Carrega `buf` (len 0 = strlen) no contexto e posiciona no primeiro token. */
short	Tjson_open(Tags *j, const char *buf, int len);
/** Como Tjson_open(), lendo de um arquivo (`file` NULL reusa o buffer atual). */
short	Tjson_start(Tags *j, const char *file);
/** Avanca para o proximo token. Retorna o tipo, ou Tags_type_end no fim. */
short	Tjson_getk_next(Tags *j);
/** Copia chave e valor do token corrente para buffers C. */
void	Tjson_gettags(Tags *j, char *tag, int stag, char *tagdata, int stagdata);
/** Le/escreve um vetor de Tpars a partir do token corrente. */
Tpars	*Tjson_get(Tags *j, Tpars *pst, short len, short flg);
/** Procura `tag` entre os tokens ja coletados; devolve o token e o valor em `buf`. */
Tjtag	*Tjson_getval(Tags *j, const char *tag, char *buf);
/** Conversor padrao de Tpars (nativo -> texto e texto -> nativo). */
short	Tjson_native(Tpars *st, char *buf, short flg);
/** Mascara printf correspondente a um Tags_type_*. */
char	*Tjson_mask_type(short type);
/** Remove escapes e brancos fora de strings. Retorna o novo comprimento. */
/**
 * Copia `src` para `dst` aplicando os escapes de string JSON (RFC 8259 par. 7).
 *
 * Trunca com seguranca se nao couber: nunca escreve alem de `dst[dstsize-1]` e
 * sempre termina com NUL. Devolve o comprimento escrito.
 */
int	Tjson_escape(char *dst, size_t dstsize, const char *src);

/**
 * Inverso de @ref Tjson_escape, no proprio buffer. Devolve o novo comprimento.
 *
 * Decodifica `\n`, `\t`, `\r`, `\b`, `\f`, `\/`, `\"`, `\\` e `\uXXXX`,
 * incluindo par surrogate, emitindo UTF-8. Sequencia invalida e' preservada
 * literal. Sempre cabe no proprio buffer: toda sequencia escapada e' mais
 * longa que o que ela representa.
 *
 * @warning NAO confunda com @ref Tjson_stripstr, que apenas remove a barra
 *          invertida -- ali `\n` vira `n`, nao quebra de linha. Para desfazer
 *          um escape de verdade, use esta.
 */
int	Tjson_unescape(char *s);

/**
 * Remove barras invertidas e caracteres de controle fora de aspas.
 *
 * @warning Nao e' o inverso de @ref Tjson_escape -- so' descarta a barra, sem
 *          traduzir a sequencia. Para decodificar, use @ref Tjson_unescape.
 */
int	Tjson_stripstr(char *buf);
/** Acumula o token corrente no vetor `t`. */
void	Tjson_data(Tags *j);
/** Primeiro caractere nao-branco a partir do cursor; avanca `j->d.p`. */
char	Tjson_nextch(Tags *j);
/** Imprime o documento reindentado em stdout (depuracao). */
void	Tjson_display(Tags *j, short flg);

/* ------------------------------------------------------------------ */
/* Escrita de JSON                                                     */
/* ------------------------------------------------------------------ */

/** Emite um par chave/valor, ou abre objeto/array quando `type` e' object/array. */
int	Tjson_tag(Tags *j, short type, const char *tag, const char *fmt, ...);
/** Fecha o nivel aberto mais recente. Retorna o nivel resultante. */
short	Tjson_closetag(Tags *j);
/** Abre um objeto nomeado. */
#define	Tjson_newtag(j, tag)		Tjson_tag(j, Tags_type_object, tag, NULL)
/** Emite uma chave com valor string formatado. */
#define	Tjson_tagdata(j, tag, args...)	Tjson_tag(j, Tags_type_string, tag, ##args)

/* ------------------------------------------------------------------ */
/* Base64                                                             */
/* ------------------------------------------------------------------ */

/** Codifica `length` bytes em base64. `buf` precisa de 4*((length+2)/3)+1 bytes. */
char	*Tjson_64encode(char *buf, const char *input, int length);
/** Decodifica base64. `buffer` precisa de 3*(strlen(input)/4)+1 bytes. */
char	*Tjson_64decode(char *buffer, const char *input);
/** Tamanho exato do texto base64 para `n` bytes de entrada (sem o NUL). */
#define	Tjson_64encode_len(n)	(4 * (((n) + 2) / 3))

/* ------------------------------------------------------------------ */
/* DOM — arvore em memoria (substitui o uso tipico da cJSON)           */
/* ------------------------------------------------------------------ */

/** Profundidade maxima de aninhamento aceita por Tjson_parse(). */
#define	Tjson_dom_depth		64

/**
 * No da arvore. Todos os nos de um documento vivem em **um unico bloco**
 * alocado por Tjson_parse(), junto com uma copia do texto — por isso
 * Tjson_free() e' um `free` so, sem travessia.
 *
 * `key` e `val` apontam para dentro dessa copia (nunca para o texto do
 * chamador, que pode ser liberado logo apos o parse).
 *
 * `val` so e' preenchido em Tags_type_string, como o `valuestring` da cJSON;
 * numeros e booleanos ficam em `num`.
 */
typedef	struct Tjnode {
	struct Tjnode	*child,		/**< Primeiro filho (objeto/array). */
			*next;		/**< Proximo irmao. */
	char		*key,		/**< Nome da chave, ou NULL em item de array. */
			*val;		/**< Texto ja desescapado (so em string). */
	double		num;		/**< Valor de numero, ou 0/1 em booleano. */
	short		type;		/**< Um dos Tags_type_*. */
	} Tjnode;

/** Parseia `txt` (len 0 = strlen). Retorna a raiz, ou NULL se malformado. */
Tjnode	*Tjson_parse(const char *txt, int len);
/** Libera o documento inteiro. So aceita a **raiz** devolvida por Tjson_parse(). */
void	Tjson_free(Tjnode *root);

/** Filho de nome `key`, sem diferenciar maiusculas (como cJSON_GetObjectItem). */
Tjnode	*Tjson_item(Tjnode *n, const char *key);
/** Filho de nome `key`, diferenciando maiusculas. */
Tjnode	*Tjson_itemcs(Tjnode *n, const char *key);
/** Filho na posicao `i`, ou NULL. */
Tjnode	*Tjson_idx(Tjnode *n, int i);
/** Numero de filhos de um objeto/array (0 nos demais tipos). */
int	Tjson_count(Tjnode *n);

/**
 * Acessores tolerantes: `n` NULL, chave ausente ou tipo incompativel devolvem
 * `def`. Com `key` NULL leem o proprio `n` — util dentro de Tjson_foreach().
 *
 * Tjson_num()/Tjson_int()/Tjson_bool() tambem aceitam o valor vindo como
 * string ("12.5", "true"), coisa que a cJSON obriga a tratar na mao. Fora
 * isso seguem a cJSON: booleano nao conta como numero, e numero fracionario
 * lido como booleano trunca antes de testar (0.5 e' falso).
 */
char	*Tjson_str(Tjnode *n, const char *key, char *def);
double	Tjson_num(Tjnode *n, const char *key, double def);
long	Tjson_int(Tjnode *n, const char *key, long def);
short	Tjson_bool(Tjnode *n, const char *key, short def);

#define	Tjson_foreach(e, n)	for((e) = (n) ? (n)->child : NULL; (e); (e) = (e)->next)
#define	Tjson_has(n, k)		(Tjson_item(n, k) != NULL)

/*
 * Predicados de tipo. Sao `static inline` e nao macros de proposito: o uso
 * natural e' Tjson_isnum(Tjson_item(r, "x")), e uma macro avaliaria a busca
 * duas vezes.
 */
static inline short	Tjson_isstr(const Tjnode *n)	{ return n && n->type == Tags_type_string; }
static inline short	Tjson_isbool(const Tjnode *n)	{ return n && n->type == Tags_type_boolean; }
static inline short	Tjson_isnull(const Tjnode *n)	{ return n && n->type == Tags_type_null; }
static inline short	Tjson_isobj(const Tjnode *n)	{ return n && n->type == Tags_type_object; }
static inline short	Tjson_isarr(const Tjnode *n)	{ return n && n->type == Tags_type_array; }
static inline short	Tjson_istrue(const Tjnode *n)	{ return Tjson_isbool(n) && n->num != 0; }
static inline short	Tjson_isnum(const Tjnode *n) {
	return n && (n->type == Tags_type_int || n->type == Tags_type_double);
}

/* ------------------------------------------------------------------ */
/* XML                                                                 */
/* ------------------------------------------------------------------ */

/**
 * Prefixo de namespace das tags geradas e atributo fixo do proximo elemento.
 * `ns` (ex.: "soap:") precede o nome em Txml_newtag()/Txml_tagdata(); na
 * leitura, conteudo que contenha "<ns" nao e' tratado como texto. `cmpl`
 * (ex.: "xmlns:x=\"...\"") entra na proxima tag aberta e e' descartado depois.
 * Os ponteiros sao guardados, nao copiados: devem sobreviver ao uso.
 */
void	Txml_setns(Tags *x, char *ns, char *cmpl);
/**
 * Remove no lugar todo comentario `<!-- ... -->` do buffer de `x`.
 * @return novo comprimento + 1, ou ERRO se um comentario nao fecha.
 */
int	Txml_uncomment(Tags *x);
/**
 * Leitura XML: avanca de `p` ate o proximo elemento com conteudo, pulando
 * comentarios, CDATA, `<?...?>` e tags de fechamento. Preenche o nome em
 * `x->x.xname`/`x->d.tag` (sem prefixo "ns:") e o texto em
 * `x->x.xdata`/`x->d.tagdata` (truncado em MAX_TAGDATALEN-1; vazio para
 * `<tag/>` ou quando o conteudo e' outro elemento).
 * @return ponteiro para continuar a varredura, ou NULL no fim do texto.
 */
char	*Txml_tagnext(Tags *x, char *p);
/**
 * Empilha uma copia do nome da tag corrente (`x->d.p` ate `x->d.tagdata.e`)
 * e sobe um nivel; Txml_closetag() usa a pilha para fechar. Uso interno de
 * Txml_newtag(), exposto para quem monta tags a mao.
 * @return a copia empilhada, ou NULL se faltar memoria.
 */
char	*Txml_getdata(Tags *x);
/**
 * Escrita XML: abre `<ns+tag atributos>` no buffer e sobe um nivel. `fmt`
 * (printf, opcional) gera os atributos; o `cmpl` de Txml_setns() entra aqui
 * e e' consumido. Com o flag Xml_tabs indenta por nivel e quebra linha.
 * Feche com Txml_closetag().
 * @return bytes escritos.
 */
int	Txml_newtag(Tags *x, const char *tag, const char *fmt, ...);
/** Fecha o elemento aberto mais recente (`</ns+tag>`); sem nada aberto, nao faz nada. */
void	Txml_closetag(Tags *x);
/**
 * Escreve um elemento folha completo: `<tag>texto</tag>`, ou `<tag/>` se o
 * texto (printf `fmt`, aparado por Tchar_check()) ficar vazio. Chama o
 * callback de `x` com Tjson_event_write antes de gravar.
 * @return bytes escritos.
 */
int	Txml_tagdata(Tags *x, const char *tag, const char *fmt, ...);

/* ------------------------------------------------------------------ */
/* Diagnostico                                                         */
/* ------------------------------------------------------------------ */

/** Loga um token (nivel, tipo, n. de elementos, posicao, tag e dado) via Tschip_log(); use a macro Tjson_log(). */
void	Tjson_out(const char *file, const char *func, int line, Tjtag *j);
#ifndef Tjson_log
#define	Tjson_log(d)	Tjson_out(__FILE__, (const char *) __PRETTY_FUNCTION__, __LINE__, d)
#endif

/* ------------------------------------------------------------------ */
/* Iteracao                                                            */
/* ------------------------------------------------------------------ */

/* O segundo argumento existe por compatibilidade com o codigo atual e e' ignorado. */
#define	Tjson_forall(j, f)	for(Tjson_start(j, NULL); (j)->d.type; Tjson_getk_next(j))
#define	Tjson_forlevel(j)	for(Tjtag t = (j)->d; t.nelem > -1; Tjson_getk_next(j), t.nelem--)
#define	Tjson_feed(j, a)	for(Tjson_start(j, NULL); (j)->d.type; Tjson_getk_next(j)) \
					Tjson_get(j, a, (short) (sizeof(a) / sizeof(*(a))), Ok)
#define	Tjson_dprintf_buf(f, j)	dprintf(f, "(%s)(%s)(%d)(%.*s)\n", __FILE__, __PRETTY_FUNCTION__, \
					__LINE__, (j)->b->s.l, (j)->b->s.d)

#ifdef __cplusplus
}
#endif

#endif
