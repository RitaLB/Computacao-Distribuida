/*
 * INE5418 - T1 - Autores: NomeA, NomeB, NomeC   <-- TODO
 * session.h - Tabela de sessões de upload (uma por cliente), chaveada por (ip, porta).
 *
 * Estado persistente em disco: <dir>/<nome>.part (dados já recebidos; o TAMANHO do
 * arquivo é o offset de retomada). Ao concluir: rename(.part -> <nome>).
 */
#ifndef SESSION_H
#define SESSION_H

#include <limits.h>
#include <netinet/in.h>
#include <stdint.h>
#include <time.h>

#include "protocol.h"

#define MAX_SESSIONS       64
#define SESSION_TIMEOUT_S  60   /* sessão inativa expira (o .part permanece em disco) */

typedef struct {
    int                in_use;
    int                done;                  /* arquivo concluído; sessão mantida p/ reenviar END_ACK */
    int                fd;                    /* .part aberto (-1 se fechado) */
    struct sockaddr_in peer;
    char               name[MAX_NAME_LEN + 1];
    char               part_path[PATH_MAX];
    char               final_path[PATH_MAX];
    uint64_t           total_size;
    uint64_t           received;              /* bytes gravados == próximo offset esperado */
    unsigned long      dups;                  /* duplicatas ignoradas (evidência p/ relatório) */
    time_t             last_activity;
} session_t;

void       sessions_init(void);
session_t *session_find(const struct sockaddr_in *peer);
session_t *session_by_name(const char *name);   /* sessão ATIVA (não concluída) com esse nome */
session_t *session_create(const struct sockaddr_in *peer);
void       session_close(session_t *s);         /* fecha fd e libera o slot */
void       sessions_gc(time_t now);

/* Aceita só basename: rejeita vazio, '/', '\\', "." e "..". 0 = ok, -1 = inválido. */
int        sanitize_filename(const char *in, size_t in_len, char *out, size_t out_sz);

#endif
