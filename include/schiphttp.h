/*
 * libschiphttp
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
 * @file schiphttp.h
 * @brief Rede do ecossistema Schip: sockets, TLS, HTTP cliente e servidor,
 *        WebSocket, cookies e parsing de URL.
 *
 * Copyright (c) 1988 Eurico Alves Schmidt <euricosalvesschmidt@gmail.com>
 * SPDX-License-Identifier: Proprietary
 *
 * Depende de <schipbase.h> (buffer e strings), <schipjson.h> (corpo JSON) e
 * do OpenSSL. Nenhum tipo de banco de dados aparece nesta interface.
 *
 * O tipo central e' o `Tflow`: um fluxo bidirecional que abstrai socket cru,
 * TLS ou arquivo local atras dos mesmos Tflow_read()/Tflow_write(). Cliente e
 * servidor usam a mesma estrutura; o papel e' escolhido por flag em
 * Tflow_new().
 *
 * Cliente:
 * @code
 *   Tbuf  *body = Tbuf_new(0);
 *   Tflow *f = Tflow_new(Tflow_flg_uri, "https://api.exemplo.com/v1");
 *   Tflow_transaction(f, body, Thttp_connection_close, NULL, "/status");
 *   Tflow_close(f);
 * @endcode
 */
#ifndef	schiphttp_h
#define	schiphttp_h		1

#include <schipbase.h>
#include <schipjson.h>

#include <openssl/ssl.h>
#include <openssl/sha.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	SCHIPHTTP_VERSION_MAJOR	1
#define	SCHIPHTTP_VERSION_MINOR	0
#define	SCHIPHTTP_VERSION_PATCH	0

/* ------------------------------------------------------------------ */
/* Compatibilidade de nomes de socket                                  */
/* ------------------------------------------------------------------ */

#define	sockreadbytes		read
#define	sockwritebytes		write
#define	closesocket		close
#define	ioctlsocket		ioctl

#define	ERRO_STARTSOCKWIN	(-2000)
#define	ERRO_HOSTBYNAME		(-2001)
#define	ERRO_OPENSOCKET		(-2002)
#define	ERRO_SOCKCONNECT	(-2003)
#define	ERRO_SOCKFLUSH		(-2004)
#define	ERRO_HOSTBYADDRESS	(-2005)

#ifndef	LF
#define	LF	10
#endif
#ifndef	CR
#define	CR	13
#endif

/* ------------------------------------------------------------------ */
/* Limites                                                             */
/* ------------------------------------------------------------------ */

#define	Thttp_max_page			1000
#define	Thttp_max_url			2083
#define	Thttp_max_endpoint		8192
#define	Thttp_max_cookies		50
#define	Thttp_max_querys		100
#define	Thttp_max_endpoints		10
#define	Thttp_default_timeout	30
#define	Thttp_max_redirects	5
/** Teto padrao de Tflow.max_body: alterar f->max_body depois de criar o
 *  fluxo para servir corpos maiores (uploads, por exemplo). */
#define	Thttp_max_body_default		(10 * 1024 * 1024)

/* ------------------------------------------------------------------ */
/* Metodos, flags e codigos                                            */
/* ------------------------------------------------------------------ */

#define	Tflow_type_method		0

#define	Tflow_method_get		0
#define	Tflow_method_post		1
#define	Tflow_method_put		2
#define	Tflow_method_delete		3
#define	Tflow_method_path		4
#define	Tflow_method_head		5
#define	Tflow_method_options		6
#define	Tflow_method_patch		7

#define	Tflow_flg_token			1
#define	Tflow_flg_tokenapi		2
#define	Tflow_flg_webhook		4

#define	Tflow_flg_send			0x000100
#define	Tflow_flg_file			0x000200	/**< O "fluxo" e' um arquivo local. */
#define	Tflow_flg_ssl_insecure		0x000400	/**< Desliga validacao TLS do cliente. */
#define	Tflow_flg_socket		0x000800
#define	Tflow_flg_websocket		0x001000
#define	Tflow_flg_ssl			0x002000
#define	Tflow_flg_uri			0x004000
#define	Tflow_flg_server		0x008000	/**< bind/listen/accept em vez de connect. */
#define	Tflow_flg_inetd			0x010000	/**< Ja veio conectado via inetd. */
#define	Tflow_flg_log			0x020000
#define	Tflow_flg_pack			0x040000
#define	Tflow_flg_raw			0x080000

#define	Thttp_connection_keep_alive	0
#define	Thttp_connection_close		1
#define	Thttp_connection_upgrade	2

#define	Thttp_content_text		0
#define	Thttp_content_application	1
#define	Thttp_content_image		2
#define	Thttp_content_font		3

#define	Thttp_type_plain		0
#define	Thttp_type_html			1
#define	Thttp_type_javascript		2
#define	Thttp_type_css			3
#define	Thttp_type_png			4
#define	Thttp_type_json			5
#define	Thttp_type_xml			6
#define	Thttp_type_gif			7
#define	Thttp_type_jpg			8
#define	Thttp_type_pdf			9
#define	Thttp_type_urlencode		10
#define	Thttp_type_csv			11
#define	Thttp_type_svg			12
#define	Thttp_type_webp			13
#define	Thttp_type_ico			14
#define	Thttp_type_jpeg			15

#define	Thttp_url_endpoint		1
#define	Thttp_url_query			2
#define	Thttp_url_up			4
#define	Thttp_url			(Thttp_url_endpoint | Thttp_url_query)

#define	Thttp_switching_protocol	101
#define	Thttp_ok			200
#define	Thttp_created			201
#define	Thttp_no_content		204
#define	Thttp_partial_content		206
#define	Thttp_moved_permanently		301
#define	Thttp_found			302
#define	Thttp_see_other			303
#define	Thttp_temporary_redirect	307
#define	Thttp_permanent_redirect	308
#define	Thttp_badrequest		400
#define	Thttp_unauth			401
#define	Thttp_forbidden			403
#define	Thttp_notfound			404
#define	Thttp_method_not_allowed	405
#define	Thttp_request_timeout		408
#define	Thttp_conflict			409
#define	Thttp_unprocessable		422
#define	Thttp_too_many_requests		429
#define	Thttp_internalerror		500
#define	Thttp_bad_gateway		502
#define	Thttp_unavailable		503
#define	Thttp_gateway_timeout		504

/**
 * Operadores de comparacao aceitos na query string (`?campo>valor`).
 *
 * Os valores coincidem com os Tfile_flg_* da SchipDB, que e' quem consome o
 * resultado do parser — nao altere sem revisar la.
 */
#define	Thttp_op_equal			0x01
#define	Thttp_op_major			0x02
#define	Thttp_op_minor			0x04
#define	Thttp_op_down			0x08
#define	Thttp_op_not_equal		0x10

/* ------------------------------------------------------------------ */
/* Estruturas                                                          */
/* ------------------------------------------------------------------ */

typedef struct Tflow Tflow;

/** Contexto TLS de uma conexao. */
typedef	struct {
	SSL		*ssl;
	SSL_CTX		*ctx;
	X509		*cert;
	X509_NAME	*certname;
	BIO		*bio,
			*certbio,
			*outbio;
	char		client_cert[1024],
			client_key[1024],
			ca_file[1024];
	} Tssl;

/** Descritor e enderecos de uma conexao. */
typedef	struct {
	intptr_t	sock;
	unsigned short	sport,
			dport;
	char		ipsrc[32],
			ipdst[32];
	} Tsock;

/**
 * Par campo/valor extraido da query string.
 *
 * `v` e' opaco para esta lib: a aplicacao guarda ali o ponteiro para o campo
 * que o nome resolve (na SchipDB, um `Tvar *`).
 */
typedef	struct {
	void	*v;		/**< Ponteiro da aplicacao (opaco aqui). */
	char	*name,		/**< Nome do campo. */
		*val,		/**< Valor como texto. */
		op;		/**< Combinacao de Thttp_op_*. */
	} Top;

/** URL decomposta. */
typedef	struct {
	int64_t	t1;
	Tsock	c;
	Top	q[Thttp_max_querys];		/**< Query string interpretada. */
	char	token[2048],
		*ep[Thttp_max_endpoints],	/**< Segmentos do caminho (dentro de `epbuf`). */
		user[32],
		password[32],
		proto[16],
		host[256],
		endpoint[Thttp_max_url],	/**< Caminho + query crus, como recebidos. */
		epbuf[Thttp_max_url];		/**< Copia de trabalho: Turl_parser_endpoint()
						     tokeniza aqui, preservando `endpoint`. */
	unsigned short	ecount,			/**< Numero de segmentos em `ep`. */
			qcount;			/**< Numero de pares em `q`. */
	} Turl;

/** Um cookie. */
typedef	struct {
	char	var[32],
		data[64],
		path[32],
		domain[64],
		expires[32],
		flag[16];
	} Tck_var;

/** Jogo de cookies de uma conexao. */
typedef	struct {
	short	itens;
	Tck_var	v[Thttp_max_cookies];
	} Tcookie;

/**
 * Um campo de um corpo multipart/form-data recebido (ver
 * Tflow_multipart_parse()). `data`/`size` apontam para dentro do `Tbuf` do
 * corpo -- sem copia -- entao so' valem enquanto esse `Tbuf` nao for
 * reaproveitado/fechado.
 */
typedef	struct {
	char	name[128],
		filename[256],		/**< Vazio quando o campo nao e' um arquivo. */
		content_type[128];	/**< Vazio quando a parte nao declarou um. */
	char	*data;
	int	size;
	} Tmultipart;

/** Contadores de uma requisicao. */
typedef	struct {
	int64_t	lcounter,	/**< Iteracoes de busca. */
		counter,	/**< Registros retornados. */
		QueryCount;	/**< Queries executadas. */
	clock_t	utime;		/**< Tempo de CPU. */
	} Tstats;

/**
 * Fluxo de dados bidirecional.
 *
 * `read`/`write` sao trocados conforme o transporte escolhido (socket cru,
 * TLS, arquivo), de modo que o resto do codigo nao muda.
 */
struct Tflow {
	unsigned char	authenticated;
	void		*db;		/**< Contexto da aplicacao (opaco aqui). */
	Turl		u;
	Tstats		s;
	unsigned char	connection,	/**< Thttp_connection_*. */
			content,	/**< Thttp_content_*. */
			type,		/**< Thttp_type_*. */
			method,		/**< Tflow_method_*. */
			opcode;
	short		flg,
			cnt,
			read_size,
			fck,
			log,		/**< Descritor do log de trafego. */
			retcode;	/**< Codigo HTTP da resposta. */
	int64_t		skip,
			page,
			max_body;	/**< Teto de corpo aceito em Tflow_http_read();
					     Thttp_max_body_default por padrao, ajustavel
					     pela aplicacao apos criar o fluxo. */
	int		argc,
			flags,		/**< Tflow_flg_*. */
			(*write)(void *, const void *, int),
			(*read)(void *, const void *, int);
	Tcookie		*ck;
	Tbuf		*body,
			*hd;		/**< Headers HTTP customizados. */
	void		*pnt;		/**< Alvo das rotinas de I/O. */
	Tssl		ssl;
	Tags		*j;		/**< Corpo JSON (libschipjson). */
	char		**argv,
			md[SHA_DIGEST_LENGTH],
			b[SCHIP_MAX_ARG],
			*formbuf;	/**< Copia de trabalho para tokenizar corpo
					     application/x-www-form-urlencoded (aloca
					     sob demanda, libera em Tflow_close). */
	};

/* ------------------------------------------------------------------ */
/* Sockets                                                             */
/* ------------------------------------------------------------------ */

/** Bytes disponiveis para leitura no descritor. */
int	Tsock_len(int handle);
/** Abre a conexao (connect ou bind/listen/accept conforme Tflow_flg_server). */
int	Tsock_open(Tflow *f);
/** Cria o socket de escuta sem aceitar, para permitir fork() antes do accept. */
int	Tsock_listen(int port);
/** Escreve os `len` bytes inteiros no socket (repete em escrita parcial e EINTR). @return `len`, ou ERRO. */
int	Tsock_write(int handle, void *buf, int len);
/** Le ate `len` bytes, repetindo ate completar ou o par fechar. @return bytes lidos (menos que `len` so' no fim da conexao), ou ERRO. */
int	Tsock_read(int handle, void *buf, int len);
/** Preenche `c->ipsrc`/`ipdst` e as portas a partir do socket conectado. */
char	*Tsock_ips(Tsock *c);

/* ------------------------------------------------------------------ */
/* TLS                                                                 */
/* ------------------------------------------------------------------ */

/** Faz o handshake TLS (cliente ou servidor conforme Tflow_flg_server). */
int	Tssl_open(Tflow *f);

/* ------------------------------------------------------------------ */
/* Fluxo                                                               */
/* ------------------------------------------------------------------ */

/** Cria um fluxo. `uri` aceita `esquema://usuario:senha@host:porta/caminho`. */
Tflow	*Tflow_new(int flags, char *uri, ...);
/** Cria fluxo TLS cliente com certificado e chave antes do handshake. */
Tflow	*Tflow_new_client_cert(int flags, const char *certificate,
					const char *private_key, char *uri, ...);
/** Cria fluxo TLS com CA opcional e certificado/chave de cliente opcionais. */
Tflow	*Tflow_new_tls(int flags, const char *ca_file,
				const char *certificate, const char *private_key,
				char *uri, ...);
/**
 * Cria um Tflow servidor sobre um socket que o chamador ja aceitou (accept()
 * proprio, tipico de um loop epoll/select com varias conexoes ao mesmo
 * tempo). Nao faz bind/listen/accept -- so' associa `sock` para leitura e
 * escrita e marca o fluxo como servidor, para que Tflow_http_read()
 * interprete a request line (metodo/endpoint/query) em vez da status line
 * do lado cliente. `flags` aceita Tflow_flg_ssl para servidor TLS (faz o
 * handshake sobre `sock`); `certificate`/`private_key` (NULL para HTTP
 * puro) dao o certificado/chave do servidor TLS, sem depender da convencao
 * /etc/ssl/live/<host>/{fullchain,privkey}.pem.
 *
 * `sock` ganha um timeout de leitura/escrita de Thttp_default_timeout
 * segundos (ajustavel via setsockopt(f->u.c.sock, ...) se precisar de outro
 * valor) e passa a ser fechado por Tflow_close(f). Devolve NULL se `sock`
 * for negativo, se a alocacao falhar ou se o handshake TLS falhar.
 */
Tflow	*Tflow_new_accepted(int flags, int sock, const char *certificate, const char *private_key);
/** Executa uma requisicao completa e devolve o proprio fluxo. */
Tflow	*Tflow_transaction(Tflow *f, Tbuf *b, unsigned char connection,
			   char *url, char *endpoint, ...);
/** Fecha o fluxo e libera tudo que ele alocou. */
void	Tflow_close(Tflow *f);

/** Le ate `size` bytes do fluxo (TCP ou TLS, pelo callback de `f`); copia para `f->log` se ligado. @return bytes lidos, 0 no fim, ERRO. */
int	Tflow_read(Tflow *f, void *data, int size);
/** Escreve os `size` bytes inteiros no fluxo (repete em escrita parcial e EINTR). @return `size`, ou ERRO. */
int	Tflow_write(Tflow *f, void *data, int size);
/** Zera `b` e le EXATAMENTE `size` bytes nele; conexao fechada antes disso e' ERRO. @return `size`, ou ERRO. */
int	Tflow_Tbuf_read(Tflow *f, Tbuf *b, int size);
/** Escreve o conteudo de `b` inteiro (Tflow_write()). */
int	Tflow_Tbuf_write(Tflow *f, Tbuf *b);
/**
 * Formata como printf e escreve no fluxo.
 * @warning usa buffer ESTATICO de SCHIP_MAX_ARG (1024) bytes: trunca texto
 * maior e nao e' seguro entre threads. Para corpo grande ou concorrencia,
 * monte num Tbuf (Tbuf_printf) e use Tflow_Tbuf_write().
 */
int	Tflow_printf(Tflow *f, char *fmt, ...);
/**
 * Le uma linha terminada em LF (CR descartado) para `buf`, sem o terminador,
 * usando o buffer interno de `f` (Tflow_flg_raw: le byte a byte). Linha maior
 * que `maxsize`-1 e' truncada, com `f->retcode` = ERRO.
 * @return caracteres lidos (a ultima linha sem LF tambem conta), ou ERRO no fim.
 */
int	Tflow_readline(Tflow *f, char *buf, int maxsize);

/** Monta e envia a requisicao, le a resposta para `body`. */
int	Tflow_http_transaction(Tflow *f, Tbuf *body);
/** Le a resposta HTTP (headers + corpo, tratando chunked e gzip) para `b`. */
int	Tflow_http_read(Tflow *f, Tbuf *b);
/** Le uma linha terminada em CRLF para o buffer. */
int	Tflow_line(Tflow *f, Tbuf *b, int size);
/**
 * Traduz sequencias de escape de uma string vinda do protocolo.
 *
 * @warning Apesar do nome, NADA tem a ver com @ref Tflow_transaction (que faz
 *          a requisicao HTTP). Esta so' mexe numa string.
 */
char	*Tflow_trans(char *src);

/** Registra um cookie recebido no cabecalho `Set-Cookie`. */
void	Tflow_set_cookie(Tflow *f, char *h);
/** Emite os cookies conhecidos no cabecalho da requisicao. */
void	Tflow_http_send_cookies(Tflow *f, Tbuf *wr);

/** Envia `data` como um quadro WebSocket de texto (opcode 1, FIN). @return `size`, ou ERRO. */
int	Tflow_websocket_write(Tflow *f, void *data, int size);
/**
 * Le a proxima mensagem WebSocket em `b` (desmascara se necessario). Responde
 * ping com pong e ignora pong sozinho; o opcode fica em `f->opcode`.
 * @return tamanho da mensagem, 0 se o par enviou close, ou ERRO (quadro
 * invalido, bits RSV ligados, controle > 125 bytes, conexao caiu).
 */
int	Tflow_websocket_read(Tflow *f, Tbuf *b);

/* ------------------------------------------------------------------ */
/* Cookies, ambiente e query                                           */
/* ------------------------------------------------------------------ */

/** Diagnostico: loga (Tschip_logw) cada cookie de `f` com path, domain, expires e flags. */
void	Tflow_list_cookies(Tflow *f);
/** Diagnostico: escreve em `fd` o estado do fluxo (host, protocolo, metodo, URL, flags, credenciais, endpoints...). Inclui a senha: nao use em log de producao. */
void	Tflow_hlog(short fd, Tflow *f);
/** Procura um campo na query string interpretada. */
Top	*Tflow_getvar(Tflow *f, char *name, char *v1, char *v2, char *v3);

/**
 * Headers da mensagem corrente, estilo CGI, numa tabela POR THREAD (ate 128)
 * que Tflow_http_read() limpa a cada mensagem -- nao use como variavel global.
 * A chave passa por Tflow_trans(): "Content-Type" vira "http_content_type".
 * Tflow_getenv() devolve o valor (NULL se ausente); Tflow_setenv() cria ou
 * substitui (copia `buf`; -1 sem memoria, tabela cheia ou `buf` NULL);
 * Tflow_unsetenv() remove (0 mesmo se ausente).
 */
char	*Tflow_getenv(char *var);
int	Tflow_setenv(char *var, char *buf);
int	Tflow_unsetenv(char *var);

/** Decodifica %XX de `src` em `dst` (pode ser o mesmo buffer; `+` NAO vira espaco). @return comprimento, limitado a SHRT_MAX. */
short	Tflow_url_decode(char *src, char *dst);
/** Codifica `src` em `dst` no estilo RFC 3986 (mantem A-Z a-z 0-9 - . _ ~). `dst` precisa de 3*strlen(src)+1 bytes. */
void	Tflow_url_encode(char *src, char *dst);

/* ------------------------------------------------------------------ */
/* URL                                                                 */
/* ------------------------------------------------------------------ */

/** Decompoe `url` em esquema, credenciais, host, porta, caminho e query. */
int	Turl_parser(Turl *u, int flags, char *url);
/** Remonta a URL a partir das partes. */
char	*Turl_construct(Tflow *f, char *buf, short flg);
/** Interpreta os segmentos de caminho e a query string em `f->u`. */
void	Turl_parser_endpoint(Tflow *f, char *endpoint);

/* ------------------------------------------------------------------ */
/* Tabelas de texto do protocolo                                       */
/* ------------------------------------------------------------------ */

/** Nome textual de um codigo: hoje so' `type` = Tflow_type_method (ex.: metodo -> "GET"); outro tipo devolve "Unknown". */
char	*Thttp_getstring(short type, short pos);

extern	Toptm	Toptm_http_retcode[],
		Toptm_http_content[],
		Toptm_http_type[],
		Toptm_http_connection[],
		Toptm_http_method[];

/* ------------------------------------------------------------------ */
/* Diagnostico                                                         */
/* ------------------------------------------------------------------ */

/** Escreve `nome.........: valor` no descritor `fd`. */
short	Thttp_print(short fd, const char *nome, const char *fmt, ...);

#define	Thttp_print_c(f, a)	Thttp_print(f, #a, "%s", a)
#define	Thttp_print_d(f, a)	Thttp_print(f, #a, "%d", a)
#define	Thttp_print_x(f, a)	Thttp_print(f, #a, "%016x", a)
#define	Thttp_print_s(f, a)	Thttp_print(f, #a, "%d:[%.*s]", (int) sizeof(a), (int) sizeof(a), a)

/**
 * Data no formato do cabecalho HTTP (RFC 7231):
 * `Sun, 06 Nov 1994 08:49:37 GMT`. `t` 0 usa a hora corrente.
 */
char	*Thttp_date(char *buf, size_t size, int64_t t);

/* ------------------------------------------------------------------ */
/* Compressao                                                          */
/* ------------------------------------------------------------------ */

/**
 * gzip (zlib, nivel padrao) de `in` para `out`; Tbuf_gzip_decompress() aceita
 * gzip ou zlib (deteccao automatica). `out` e' zerado antes. @return Ok/ERRO.
 */
int	Tbuf_gzip_compress(Tbuf *in, Tbuf *out);
int	Tbuf_gzip_decompress(Tbuf *in, Tbuf *out);

/** Adiciona um campo de texto a um corpo multipart/form-data. */
int	Tbuf_multipart_field(Tbuf *body, const char *boundary,
				 const char *name, const void *data, int size);
/** Adiciona um arquivo binario a um corpo multipart/form-data. */
int	Tbuf_multipart_file(Tbuf *body, const char *boundary,
				const char *name, const char *filename,
				const char *content_type);
/** Fecha o corpo multipart/form-data com o boundary final. */
int	Tbuf_multipart_end(Tbuf *body, const char *boundary);
/** Adiciona o Content-Type multipart ao conjunto de headers da requisicao. */
int	Tflow_multipart_header(Tflow *f, const char *boundary);
/**
 * Extrai os campos de um corpo multipart/form-data recebido (lado servidor)
 * para `parts` (ate `max` campos). `content_type` e' o valor cru do header
 * Content-Type da requisicao (usado so' para achar o boundary). Devolve a
 * quantidade de campos encontrados, ou ERRO se o Content-Type nao tiver
 * boundary ou o corpo estiver malformado.
 * @warning insere terminadores dentro de `body`: nao leia `body->s.d` como
 * texto cru depois de chamar isto.
 */
int	Tflow_multipart_parse(Tbuf *body, const char *content_type,
				Tmultipart *parts, int max);

/** Formata uma resposta padronizada de erro JSON. */
int	Thttp_json_error(Tflow *f, int code, const char *codigo, const char *mensagem, const char *campo);

#ifdef __cplusplus
}
#endif

#endif
