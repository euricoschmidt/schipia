/*
 * include
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
 * @file schiptree.h
 * @brief libschiptree — ultra-compact key->recid index over a memory-mapped file.
 *
 * Sparse tree of 2 key bits per level (4 entries per node) with a flat root
 * directory, stored in a single file mapped with
 * MAP_SHARED. The index does NOT store the keys: each leaf holds only the
 * record's `recid`, and the key is retrieved through the application's
 * MANDATORY callback — the on-disk cost per key is independent of keylen.
 *
 * Essential API: Tschiptree_open(), Tschiptree_close(), Tschiptree_action(),
 * Tschiptree_start(), Tschiptree_find().
 *
 * Concurrency: safe across processes via a robust mutex stored in the header
 * (EOWNERDEAD handled); reads never lock, and with a single process
 * (users == 1) no lock is taken at all. Insertion is publish-then-link: the
 * process dying at any point never corrupts the tree.
 */

#ifndef SCHIPTREE_H
#define SCHIPTREE_H
/** @brief Enable GNU extensions (mremap, robust mutex). */
#define _GNU_SOURCE

#include<stdlib.h>
#include<stdint.h>
#include<string.h>	/* memcpy: used by the inline Tschiptree_rec_get/put */
#include<pthread.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>	/* close, ftruncate, fdatasync: glibc pulls it in indirectly, musl does not */
#include<errno.h>
#include<signal.h>
#include<syslog.h>

typedef	unsigned char		uchar;  /**< Unsigned byte. */
typedef	unsigned int		uint;   /**< Unsigned integer (32 bits). */

/**
 * @brief Signed record/node id — the ONLY on-disk cell of the index.
 *
 * Every entry of every node is exactly one Trec (8 bytes, little-endian host
 * order). The former variable-width "slot" (2-8 bytes) was removed: a fixed
 * 8-byte cell costs more disk but removes the width from every read, write,
 * offset and buffer size, which is both faster and far less code.
 */
typedef	int64_t	Trec;

/*
 * NAO subir esta versao por causa de formato de INDICE.
 *
 * `Tschiptree_map_init` e' a camada de armazenamento mapeado dos ARQUIVOS DE
 * DADOS tambem (dbschip.c chama com `f->recsize` como keylen), e e' ela quem
 * valida este numero. Subi-lo declara todo arquivo de dado incompativel -- e
 * dado nao e' derivado, nao ha' como reconstruir. Medido na pratica: o banco
 * inteiro parou de abrir em `sets/table`, nao so' os indices.
 *
 * Mudanca que afeta so' o INDICE se sinaliza por bit persistido no `setup`
 * (ver #Tschiptree_flg_duplsb), que so' existe onde ha' duplicata.
 */
#define	Tschiptree_version		10000			/**< On-disk format version (validated on open). */
/* Fanout (key bits consumed per level, levels > 0) is FIXED at 2 bits/level
 * (4 entries per node) as a compile-time constant: it is the best cost/benefit
 * across every measured workload (a wider fanout only shrinks the index for
 * sequential loads and inflates it under random ones), and being constant lets
 * the compiler fold the shifts/masks and loop bounds on the hot path. The old
 * runtime fanout-4 option (#Tschiptree_flg_fast) was removed; its header bit is
 * kept reserved for future use, and an index persisted with it is refused on
 * open instead of misread. */
#define	Tschiptree_node_bits		2			/**< Fixed fanout: key bits per non-root level. */
#define	Tschiptree_node_itens		(1 << Tschiptree_node_bits)	/**< Entries per internal node (4). */
#define	Tschiptree_recsize		(Tschiptree_node_itens * (int)sizeof(Trec))	/**< Bytes per node (32): fixed, node_itens Trec cells. */
#define	Tschiptree_recsize_32		(Tschiptree_node_itens * (int)sizeof(int32_t))	/**< Bytes per 32-bit node (16): node_itens 32-bit cells. */

/**
 * @name Entry flag bit
 * The top bit of a Trec cell distinguishes a child pointer (set) from a leaf
 * recid (clear); an entry equal to #Tschiptree_pos_null is empty. Cells are read
 * as signed 64-bit, so a flagged value is simply negative.
 * @{ */
#define	Tschiptree_pos_null		((Trec)(~UINT64_C(0) << 63))	/**< "Empty entry" / flag bit (INT64_MIN). */
#define	Tschiptree_pos_mask		(~Tschiptree_pos_null)		/**< Mask of the id without the flag bit (INT64_MAX). */
/** @} */

/**
 * @name Flat root directory
 * Level 0 consumes root_bits key bits in a SINGLE access (2^root_bits
 * entries), in place of root_bits/2 pointer levels — the following levels keep
 * using 2 bits. Fixed cost of (2^root_bits)*8 bytes per file (root 16 =
 * 512 KB), identical under any workload. root_bits is even (2..16), passed as
 * the 3rd argument of Tschiptree_open() at creation and read from the header on
 * subsequent opens.
 * @{ */
#define	Tschiptree_root_min		2	/**< Smallest root_bits (equals the classic layout). */
#define	Tschiptree_root_max		16	/**< Largest root_bits. */
#define	Tschiptree_root_default		12	/**< 4096 entradas (32 KiB): menos niveis em OLTP; gravado por indice, sem afetar os existentes. */
/** @} */

#define	Tschiptree_ok			0	/**< Success. */
#define Tschiptree_erro			-1	/**< Failure, missing key, or end of scan. */
#define	Tschiptree_false		0	/**< Boolean false. */
#define	Tschiptree_true			1	/**< Boolean true. */
#define	Tschiptree_null			0	/**< Null pointer/value. */

#define	Tschiptree_mmap_min_size	512 		/**< Mapping granule (avoids overhead on very small files). */

/**
 * @name Tschiptree_action() actions
 * @{ */
#define	Tschiptree_flg_newrec	0x00	/**< Insert key -> recid. */
/**< Point lookup (also positions the cursor).
 *
 * Com #Tschiptree_flg_duplicate e `recid == 0`, devolve UMA ocorrencia do
 * grupo -- nao necessariamente a de menor recid. Ate' a versao 10000 do
 * formato era a menor, porque o sufixo era big-endian; hoje o sufixo poe o
 * byte baixo na frente para nao pagar um corredor de no's por chave duplicada,
 * e a ordem DENTRO do grupo deixou de ser a de recid. Quem precisa de uma
 * ocorrencia especifica passa o recid explicito, que continua exato. */
#define	Tschiptree_flg_getkey	0x01
#define	Tschiptree_flg_delkey	0x02	/**< Remove ONE occurrence of the key. */
/** @} */

/**
 * @brief Header persisted at the start of the index file.
 *
 * Shared between processes through the mmap itself: the robust mutex and the
 * counters live inside it.
 */
typedef	struct {
	int	magic,		/**< Signature #Tschiptree_magic. */
		keylen;		/**< Fixed key length in bytes. */
	Trec	version,	/**< Format version (#Tschiptree_version). */
		count,		/**< Highest node id in use in the file. */
		users,		/**< Processes with the index open. */
		hdrlen,		/**< sizeof of the written header (layout validation). */
		deleted[2];	/**< Head [0] and tail [1] of the free-node list. */
	uchar	setup,		/**< Persistent flags (#Tschiptree_flg_mask). */
		dirty,		/**< 1 = mutation happened without a clean close (opens in sanitize mode). */
		root_bits;	/**< Level-0 width in bits (even, 2..16). */
	uint	gen;		/**< Low 31 bits: generation; high bit: transient writer marker. */
	pthread_mutex_t mutex;	/**< Robust process-shared mutex (EOWNERDEAD handled). */
} Tschiptree_map_header;

/**
 * @brief Low-level view of the mapped file.
 *
 * Binary-compatible prefix of #Tschiptree: the Tschiptree_map_* routines accept
 * either of the two via cast.
 */
typedef	struct {
	void	*map;			/**< mmap base. */
	Tschiptree_map_header	*h;	/**< Header (start of the mapping). */
	off_t	mapped_size,		/**< Currently mapped bytes. */
		offset,			/**< Start of the node area (= hdrlen). */
		recsize;		/**< Bytes per node (#Tschiptree_recsize for an index). */
	int	pnt,			/**< File descriptor. */
		lock;			/**< #Tschiptree_true if this handle holds the mutex. */
} Tschiptree_map;

/**
 * @brief Handle of an open index.
 *
 * The leading fields mirror #Tschiptree_map (casts between the two are used
 * internally). The rest is per-handle state: key buffers, the cursor stack
 * and parameters derived from the header by Tschiptree_config().
 */
typedef struct {
	void	*map;			/**< mmap base (see #Tschiptree_map). */
 	Tschiptree_map_header	*h;	/**< Shared header. */
	off_t	mapped_size,		/**< Currently mapped bytes. */
		offset,			/**< Start of the node area. */
		recsize;		/**< Bytes per node (#Tschiptree_recsize). */
	int	pnt,			/**< File descriptor. */
		lock;			/**< #Tschiptree_true if this handle holds the mutex. */
	uchar	reset_page[Tschiptree_recsize];	/**< Empty-node image (memcpy/memcmp). */
	uchar	*tmp,			/**< Normalized (lsb) and/or composite (duplicates) key. */
                *lastkey;		/**< Last key visited (base of the incremental descent). */
	int	*pniv;			/**< Cursor stack: entry index per level. */
	short	cnt,			/**< Current cursor level. */
		level_max,		/**< Maximum depth for the keylen. */
		keylen,			/**< Key length in bytes. */
		size;			/**< keylen + duplicate suffix (when enabled). */
	uchar	*lastkey_dup;		/**< recid suffix of the composite key (sizeof(Trec) bytes). */
	Trec	*ppos;			/**< Cursor stack: node id per level. */
	uchar	*page_data;		/**< Pointer to the current node in the map. */
	char	flg,			/**< Effective flags (header | open). */
		next;			/**< Cursor direction (#Tschiptree_flg_next / #Tschiptree_flg_prev). */
	/* signed: char is unsigned on ARM64/PowerPC, where -1 would read as 255. */
	signed char
		/**
		 * Resultado da ultima busca que NAO encontrou a chave: +1 se a folha
		 * onde a descida parou e' MAIOR que a chave procurada, -1 se e'
		 * menor, 0 se parou numa entrada vazia.
		 *
		 * Tschiptree_start() precisa disto para posicionar o cursor: sem
		 * saber de que lado a folha caiu, uma varredura que comeca numa
		 * chave inexistente pula o primeiro registro que casa. A informacao
		 * se perdia no `memcpy(l, v, k->size)` do fim de Tschiptree_action().
		 */
		lastcmp;
	/* Root directory (derived from h->root_bits on open) */
	short	rootlev,	/**< 2-bit levels the root replaces (root_bits/2). */
		rootshift,	/**< Shifts the byte pair down to the root index. */
		rootidx1;	/**< 2nd key byte read by the root (0 if root <= 8 bits). */
	int	root_itens;	/**< Root entries (1 << root_bits). */
	Trec	root_units;	/**< Node ids occupied by the root. */
	uchar	*scan_key;	/**< External bytes of the last stable scan anchor. */
	Trec	scan_recid;	/**< Exact duplicate suffix of the stable anchor; negative after
				 *   Tschiptree_find_recid(): scan_key is then a path prefix of
				 *   -(scan_recid + 1) bits, not a real key. */
	uint	scan_gen;	/**< Generation in which the cursor position was valid. */
	uchar	scan_has_key,	/**< scan_key contains a start/returned key. */
		scan_returned;	/**< Anchor was already emitted and must be skipped. */
	long	ptuser,					/**< Opaque value passed through to the callback. */
		(*callback)(long, void *, int, long);	/**< callback(ptuser, buf, keylen, recid): copies the key of `recid` into `buf`. MANDATORY. */
	uint	path_gen;	/**< Generation validating the cached lookup path; UINT_MAX forces a root lookup. */
} Tschiptree;

/**
 * @brief Instantaneo da POSICAO DE LEITURA de um handle.
 *
 * Existe para que uma varredura possa ser interrompida por outra sobre a MESMA
 * arvore e depois retomada de onde estava. Reancorar pela chave nao serve: com
 * duplicatas, `Tschiptree_start` volta ao INICIO do grupo, e o laco externo
 * refaz as mesmas linhas para sempre. Alem da pilha, o instantaneo guarda a
 * ancora estavel `(chave, recid, geracao)`: se a arvore mudar, a pilha e'
 * descartada e a posicao e' reconstruida desde a raiz.
 *
 * `page_data` NAO e' guardado de proposito: e' ponteiro para dentro do mmap,
 * que pode ser remapeado no intervalo. Ele e' recalculado a partir de
 * `ppos[cnt]` na restauracao.
 */
typedef struct {
	Trec	*ppos;		/**< Copia da pilha de ids de no'. */
	int	*pniv;		/**< Copia da pilha de indices por nivel. */
	uchar	*lastkey;	/**< Copia da ultima chave visitada (com o sufixo). */
	uchar	*scan_key;	/**< Chave externa da ancora estavel da varredura. */
	Trec	scan_recid;	/**< Recid exato da ancora (duplicatas). */
	uint	scan_gen;	/**< Geracao da posicao salva. */
	short	cnt,		/**< Nivel corrente do cursor. */
		nlev,		/**< Niveis copiados (confere compatibilidade). */
		size;		/**< Bytes de `lastkey` (idem). */
	char	next;		/**< Direcao da varredura. */
	signed char lastcmp;	/**< Lado em que a ultima descida parou (-1/0/+1; signed: char e' unsigned no ARM64). */
	char	scan_has_key,
		scan_returned;
} Tschiptree_cursor;

/** Guarda a posicao de leitura de `k` em `c`. Tschiptree_true se conseguiu. */
int	Tschiptree_cursor_save(Tschiptree *k, Tschiptree_cursor *c);
/** Devolve a posicao guardada. Tschiptree_true se conseguiu. */
int	Tschiptree_cursor_restore(Tschiptree *k, const Tschiptree_cursor *c);
/** Libera o que Tschiptree_cursor_save alocou. */
void	Tschiptree_cursor_free(Tschiptree_cursor *c);

/**
 * @name Flags for Tschiptree_start() and Tschiptree_open()
 * @{ */
#define	Tschiptree_flg_prev		0x00	/**< Cursor in descending order (Tschiptree_start). */
#define	Tschiptree_flg_next		0x01	/**< Cursor in ascending order (Tschiptree_start). */
#define	Tschiptree_flg_lsb		0x02	/**< Little-endian numeric keys (keylen 2, 4, 8 or 16); persistent. */
#define	Tschiptree_flg_fast		0x04	/**< RESERVED (removed): former fanout-4 flag. Fanout is fixed at 2; an index persisted with this bit is refused on open. Kept reserved for future use. */
#define	Tschiptree_flg_duplicate	0x08	/**< Allow duplicate keys (internal recid suffix); persistent. */
#define	Tschiptree_flg_nolocks		0x10	/**< Never take the mutex (strictly single-process use). */
#define	Tschiptree_flg_new		0x20	/**< Create/truncate the index file. */
#define	Tschiptree_flg_rec32		0x40	/**< 32-bit node cells (recsize 16B; max 2B recids/nodes); persistent. */
#define	Tschiptree_flg_update		Tschiptree_flg_rec32	/**< Reserved/legacy alias. */
/**
 * Sufixo do recid gravado com o byte MENOS significativo primeiro.
 *
 * Persistente e definida pela BIBLIOTECA na criacao de todo indice com
 * #Tschiptree_flg_duplicate -- quem chama nao precisa saber dela. Serve para
 * distinguir um indice antigo (sufixo big-endian) de um novo: como a mudanca
 * e' de POSICIONAMENTO, um binario novo lendo um indice velho nao acharia as
 * entradas e a linha sumiria em silencio. Sem a marca, o open RECUSA e quem
 * chama reconstroi o indice a partir dos dados.
 */
#define	Tschiptree_flg_duplsb		0x80
/** @} */

#define	Tschiptree_setup_rec32		Tschiptree_flg_rec32	/**< Bit 6 in header setup: 32-bit node cells. */
#define	Tschiptree_flg_mask		(Tschiptree_flg_duplicate|Tschiptree_flg_lsb|Tschiptree_flg_duplsb|Tschiptree_flg_rec32)	/**< Flags written to the header (setup). */

#define	Tschiptree_magic		0x62951413	/**< Index-file signature. */

/** @brief Byte offset of node `k` within the file. */
#define	Tschiptree_toseek(f, k)		(f->offset + (((off_t) k - 1) * f->recsize))

/**
 * @brief Read one #Trec cell from `p` (8 bytes, little-endian host order).
 *
 * The pointer need not be aligned: the memcpy compiles to a single load. On-disk
 * cells are plain #Trec, so a flagged (child-pointer) value comes back negative
 * with no extra work.
 */
static inline
Trec	Tschiptree_rec_get(const uchar *p) {
	Trec	v;

	memcpy(&v, p, sizeof(v));
	return v;
}
/** @brief Write one #Trec cell to `p` (8 bytes, little-endian host order). */
static inline
void	Tschiptree_rec_put(uchar *p, Trec v) {
	memcpy(p, &v, sizeof(v));
}
/**
 * @brief Ordered-scan loop: position with Tschiptree_start() and iterate while
 *        Tschiptree_find() returns a valid recid in `r`.
 * @param f   Handle (#Tschiptree*).
 * @param k   Start key (or NULL for begin/end).
 * @param flg #Tschiptree_flg_next or #Tschiptree_flg_prev.
 * @param r   #Trec variable that receives each recid.
 */
#define	Tschiptree_for_find(f, k, flg, r)	for(Tschiptree_start(f, k, flg); (r = Tschiptree_find(f)) > 0;)
/** @brief Ordered-scan loop returning only recids (skips key callback). */
#define	Tschiptree_for_find_recid(f, k, flg, r)	for(Tschiptree_start(f, k, flg); (r = Tschiptree_find_recid(f)) > 0;)
/** @brief Empties the current cursor node (copies the empty-node image). */
#define	Tschiptree_reset_page(k)		memcpy((k)->page_data, (k)->reset_page, Tschiptree_recsize)

/**
 * @brief Open (or create, with #Tschiptree_flg_new) an index in a mapped file.
 *
 * @param path      Path to the index file.
 * @param keylen    Fixed key length in bytes (> 0 at creation; on an existing
 *                  file it must match the header, 0 accepts the stored value).
 *                  With #Tschiptree_flg_lsb: 2, 4, 8 or 16.
 * @param root_bits CREATION width of the flat root directory, in bits (even,
 *                  #Tschiptree_root_min..#Tschiptree_root_max; 0 =
 *                  #Tschiptree_root_default). Written to the header at creation
 *                  and ignored when opening an existing index.
 * @param pnt       Opaque user value, passed as the 1st argument of the callback.
 * @param callback  MANDATORY: `callback(pnt, buf, keylen, recid)` must copy the
 *                  REAL stored key of record `recid` into `buf` (keylen bytes),
 *                  fetched from external storage — never derived from the recid.
 *                  Called only on a descent collision.
 * @param flg       Combination of #Tschiptree_flg_new, #Tschiptree_flg_lsb,
 *                  #Tschiptree_flg_duplicate and #Tschiptree_flg_nolocks. The
 *                  persistent flags (lsb, duplicate) are recorded at creation and
 *                  ignored on later opens. #Tschiptree_flg_fast is removed
 *                  (reserved): an index carrying it is refused.
 * @return Allocated handle, or NULL on failure (reason via syslog/errno).
 * @warning A callback that computes the key from the `recid` (e.g. `key = recid`)
 *          breaks correctness. With #Tschiptree_flg_duplicate the leaf key is
 *          `real key + recid`, so such a callback makes every getkey/delete
 *          return #Tschiptree_erro while inserts still create leaves.
 * @warning CONCURRENCY (validated by tests/stress_concurrent.c): use one handle
 *          per thread — a handle holds a private cursor and is not shareable.
 *          Multiple handles (threads or processes) on the same index are safe:
 *          writers are serialized by a robust process-shared mutex and the file
 *          only ever grows on the mutation path (extend-only), so the tree never
 *          corrupts under concurrent writers and readers (soak: 12 writers + 8
 *          readers, hundreds of millions of ops, integrity intact).
 *          One caveat remains from lock-free reads: a getkey running concurrently
 *          with a writer may observe a torn cell and pass the callback a stale or
 *          out-of-range `recid`. A torn read resolves to at most a spurious "not
 *          found" (never a wrong hit), but the callback MUST validate `recid`
 *          against its record range and not dereference it unchecked — otherwise
 *          it can segfault. When reads and writes never overlap, no validation is
 *          needed.
 */
Tschiptree	*Tschiptree_open(char *path, int keylen, uint root_bits, long pnt, long (*callback)(long, void *, int, long), short flg);

/**
 * @brief Abre um segundo handle independente sobre o mesmo arquivo de `src`.
 *
 * O mmap do arquivo continua compartilhado pelo kernel, mas cursor, buffers e
 * descritor pertencem ao novo handle. E' a forma barata e segura de dar dois
 * consumidores logicos a mesma arvore: salvar/restaurar cursor nao torna um
 * unico handle reentrante.
 *
 * A callback e' herdada; `pnt` substitui o contexto opaco, permitindo que cada
 * abertura aponte para o seu proprio descritor de chave.
 */
Tschiptree	*Tschiptree_reopen(Tschiptree *src, long pnt);

/**
 * @brief Close the low-level view: decrement `users`, msync, and — in the last
 *        process — clear the dirty flag and truncate the free tail.
 * @note Internal use — the application should call Tschiptree_close().
 */
void	Tschiptree_map_close(Tschiptree_map *m);

/** @brief Close the index (Tschiptree_map_close()) and free the handle and its buffers. */
void	Tschiptree_close(Tschiptree *e);

/**
 * @brief Advance the cursor and return the next recid in key order.
 * @return recid (> 0), or #Tschiptree_erro at the end of the scan.
 * @note Position first with Tschiptree_start(); see Tschiptree_for_find().
 */
Trec	Tschiptree_find(Tschiptree *k);

/**
 * @brief Advance the cursor and return only the next recid in key order.
 * @return recid (> 0), or #Tschiptree_erro at the end of the scan.
 * @note Never calls the key callback: the cursor anchor is rebuilt from the
 *       tree path, so deleting the current record or reusing its recid during
 *       the scan (this handle or another process) cannot move the cursor.
 *       Under writes it never repeats a row nor skips one that existed when
 *       the scan passed it; a key inserted under the same path prefix as the
 *       last returned row, after it was returned, is not visited
 *       (Tschiptree_find() would visit it). Both calls can be mixed in a scan.
 */
Trec	Tschiptree_find_recid(Tschiptree *k);

/** @brief Highest node id in use in the file (not the number of keys). */
Trec	Tschiptree_get_count(Tschiptree *k);

/**
 * @brief Forca as paginas sujas do indice para o disco (`msync(MS_SYNC)`).
 *
 * A biblioteca NAO sincroniza sozinha em nenhum caminho: escrita vai para o
 * mapa e quem decide quando a pagina desce e' o kernel. Esta funcao existe
 * para quem quer durabilidade por commit e aceita o preco -- medido neste
 * hardware (NVMe em RAID1 por software), cerca de 1 ms por flush.
 *
 * @return #Tschiptree_ok, ou #Tschiptree_erro.
 */
int	Tschiptree_sync(Tschiptree *k);

/**
 * @brief Structural checker (fsck-lite).
 *
 * Walks the whole tree validating child ids within range, absence of
 * cycles/node sharing, maximum depth and the free list. Uses its own stack
 * (does not touch the cursor state).
 * @return Number of reachable leaves (recids), or #Tschiptree_erro on the first
 *         violation.
 * @warning Run without concurrent writers.
 */
Trec	Tschiptree_check(Tschiptree *k);

/**
 * @brief Remove ALL occurrences of the key (in duplicate mode, the whole
 *        group; without duplicate, at most one).
 * @return Number of occurrences removed.
 */
Trec	Tschiptree_delete_key(Tschiptree *k, void *key);

/**
 * @brief Index generation counter (increments on every published mutation).
 * @note Useful to invalidate application caches across processes.
 */
uint	Tschiptree_get_generation(Tschiptree *k);

/**
 * @brief Position the ordered-scan cursor.
 *
 * @param k    Open handle.
 * @param key  Start key (need not exist: the cursor stops at the matching
 *             ordered position), or NULL for the begin/end of the index.
 * @param next #Tschiptree_flg_next (ascending) or #Tschiptree_flg_prev (descending).
 * @return #Tschiptree_ok.
 */
Trec	Tschiptree_start(Tschiptree *k, void *key, char next);

/**
 * @brief Point operation on a key: insert, look up, or remove.
 *
 * Semantics per action:
 * - #Tschiptree_flg_newrec — insert `key` -> `recid` (`recid` > 0, positive as a
 *   64-bit signed value). Returns `recid`; #Tschiptree_erro if the key already
 *   exists (without #Tschiptree_flg_duplicate) or if the key+recid pair already
 *   exists (with it).
 * - #Tschiptree_flg_getkey — point lookup. In duplicate mode, `recid` addresses
 *   the exact occurrence and 0 returns the lowest-recid one. Returns the found
 *   recid or #Tschiptree_erro. Also positions the cursor.
 * - #Tschiptree_flg_delkey — remove ONE occurrence. In duplicate mode, `recid`
 *   selects which one (0 = the lowest-recid one). Returns the removed recid or
 *   #Tschiptree_erro. To remove the whole group use Tschiptree_delete_key().
 *
 * Concurrency: mutations take the robust mutex only when `users > 1`; lookups
 * never lock.
 *
 * @param k      Open handle.
 * @param key    Key (keylen bytes).
 * @param recid  Record id (per-action semantics, above).
 * @param action #Tschiptree_flg_newrec, #Tschiptree_flg_getkey or #Tschiptree_flg_delkey.
 * @return recid (> 0) on success; #Tschiptree_erro otherwise.
 */
Trec	Tschiptree_action(Tschiptree *k, void *key, Trec recid, char action);

/**
 * @name Convenience wrappers over Tschiptree_action()
 * Zero-cost inlines that name the intent at the call site; same semantics and
 * return values as the corresponding action above.
 * @{
 */
/** @brief Insert `key` -> `recid` (#Tschiptree_flg_newrec). */
static inline
Trec	Tschiptree_insert(Tschiptree *k, void *key, Trec recid) {
	return Tschiptree_action(k, key, recid, Tschiptree_flg_newrec);
}
/** @brief Point lookup (#Tschiptree_flg_getkey); in duplicate mode returns the lowest-recid occurrence. */
static inline
Trec	Tschiptree_get(Tschiptree *k, void *key) {
	return Tschiptree_action(k, key, 0, Tschiptree_flg_getkey);
}
/** @brief Remove ONE key+recid occurrence (#Tschiptree_flg_delkey); for the whole group use Tschiptree_delete_key(). */
static inline
Trec	Tschiptree_delete_rec(Tschiptree *k, void *key, Trec recid) {
	return Tschiptree_action(k, key, recid, Tschiptree_flg_delkey);
}
/** @} */

/** @brief Empty the index (reset count and free list, re-empty the root) keeping file and handle. */
Tschiptree*	Tschiptree_reset(Tschiptree *k);

/**
 * @name Low-level API (Tschiptree_map_*)
 * Internal routines exposed for tools/tests; normal applications use only the
 * main API above.
 * @{ */

/**
 * @brief Ensure the mapping (and file size) up to node `flatkey`, growing via
 *        ftruncate + mremap when needed.
 * @return `flatkey`, or #Tschiptree_erro on failure.
 */
Trec	Tschiptree_map_setmem(Tschiptree_map *m, Trec flatkey);

/**
 * @brief Take (status #Tschiptree_true) or release (#Tschiptree_false) the header
 *        mutex, handling EOWNERDEAD (dead owner -> mutex_consistent).
 * @return #Tschiptree_true with the lock held, #Tschiptree_ok on release,
 *         #Tschiptree_erro on failure.
 */
int	Tschiptree_setlock(Tschiptree_map *k, uchar status);

/**
 * @brief Map the file and create/validate the header (magic, version, keylen,
 *        root), sanitize the index after a crash (dirty) and register the
 *        process in `users`.
 * @param m         View with `pnt` (fd) already open.
 * @param keylen    Key length (see Tschiptree_open()).
 * @param root_bits Creation root width in bits (see Tschiptree_open()).
 * @param flg       Persistent flags (filtered by #Tschiptree_flg_mask).
 * @return #Tschiptree_ok or #Tschiptree_erro.
 */
int	Tschiptree_map_init(Tschiptree_map *m, int keylen, uint root_bits, uchar flg);

/**
 * @brief How many indices this process had to sanitize on open (dirty header).
 *
 * A sanitized index is writable again but its CONTENTS may be incomplete --
 * a lookup can miss a live row, a scan can repeat one. The caller is expected
 * to rebuild it from the source data.
 *
 * @param clear non-zero resets the counter after reading it.
 * @return the counter value before the optional reset.
 */
int	Tschiptree_sanitized(int clear);

/**
 * @brief Take the mutex only if there are other processes (`users > 1`).
 * @return #Tschiptree_true if taken, #Tschiptree_false if unnecessary,
 *         #Tschiptree_erro on failure.
 */
int	Tschiptree_map_lock(Tschiptree_map *m);

/** @brief Release the mutex if `locked` == #Tschiptree_true (pair of Tschiptree_map_lock()). */
void	Tschiptree_map_unlock(Tschiptree_map *m, int locked);

/** @brief Put node `nodeid` at the tail of the free list (for reuse). */
void	Tschiptree_map_deleted_add(Tschiptree_map *m, Trec nodeid);

/** @brief Take and return the node at the head of the free list (0 if empty). */
Trec	Tschiptree_map_deleted_remove(Tschiptree_map *m);

/**
 * @brief Return the contiguous free nodes at the end back to the file,
 *        reducing `count` down to at most `min_count` (the physical truncation
 *        happens on the last close).
 */
void	Tschiptree_map_deleted_trim_file_end(Tschiptree_map *m, Trec min_count);

/**
 * @brief Shrink the file to the current high-water `count` at runtime, without
 *        closing, when this is the sole open handle (exclusive lock granted).
 *
 * Reclaims the pre-allocated tail (chunk growth or an emptied index) that the
 * close-time shrink would only return on the last clean close. If any other
 * fd/process has the index open the exclusive lock is denied and nothing is
 * touched, so it is safe to run against a live database (busy files are simply
 * skipped).
 *
 * @return Bytes freed (>= 0), or -1 when not the sole owner or on error.
 * @warning After a successful trim the mapping may exceed the file; do not grow
 *          the index through this handle without reopening. Intended for a
 *          maintenance pass followed by close/exit.
 */
off_t	Tschiptree_map_trim(Tschiptree_map *m);

/** @} */

#include <syslog.h>

/** @brief Standard log (syslog LOCAL6) with file, function and line. */
#define Tschiptree_log(fmt, ...) syslog(LOG_WARNING | LOG_LOCAL6, "%s:%s:%d: " fmt, __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#endif /* SCHIPTREE_H */
