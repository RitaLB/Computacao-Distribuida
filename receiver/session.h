/*
 * INE5418 - T1 - Autores: NomeA, NomeB, NomeC   <-- TODO
 * session.h - Tabela de sessões de upload (uma por cliente ativo).
 *
 * Chave da sessão: endereço (IP:porta) de origem do cliente.
 * Estado persistente em disco (permite retomar):
 *   <dir>/<nome>.part       dados já recebidos (tamanho = offset de retomada)
 *   <dir>/<nome>.part.meta  tamanho total esperado (detecta arquivo diferente com mesmo nome)
 * Ao concluir: rename(<nome>.part -> <nome>) e remove o .meta.
 */
#ifndef SESSION_H
#define SESSION_H

#include <limits.h>
#include <netinet/in.h>
#include <stdint.h>
#include <time.h>

#include "protocol.h"

#define MAX_SESSIONS       64
#define SESSION_TIMEOUT_S  60      /* sessão sem atividade é encerrada (o .part permanece) */

typedef struct {
    int                in_use;
    struct sockaddr_in peer;
    int                fd;                    /* descritor do .part aberto */
    char               name[MAX_NAME_LEN + 1];
    char               part_path[PATH_MAX];
    char               meta_path[PATH_MAX];
    char               final_path[PATH_MAX];
    uint64_t           total_size;
    uint64_t           received;              /* bytes já gravados == próximo offset esperado */
    time_t             last_activity;
} session_t;

void       sessions_init(void);
session_t *session_find(const struct sockaddr_in *peer);
session_t *session_create(const struct sockaddr_in *peer);      /* NULL se tabela cheia */
void       session_close(session_t *s);                         /* fecha fd e libera o slot */
void       sessions_gc(time_t now);                             /* expira sessões inativas */
int        name_in_use(const char *name);                       /* outra sessão já usa esse nome? */

/* Aceita só o basename: rejeita vazio, '/', '\\', "." e "..". 0 = ok, -1 = inválido. */
int        sanitize_filename(const char *in, size_t in_len, char *out, size_t out_sz);

#endif /* SESSION_H */
