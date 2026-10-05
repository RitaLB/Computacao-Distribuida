/*
 * INE5418 - T1 - Autores: NomeA, NomeB, NomeC   <-- TODO
 * session.c - implementação da tabela de sessões.
 */
#include "session.h"

#include <string.h>
#include <unistd.h>

static session_t table[MAX_SESSIONS];

void sessions_init(void)
{
    memset(table, 0, sizeof table);
    for (int i = 0; i < MAX_SESSIONS; i++) table[i].fd = -1;
}

session_t *session_find(const struct sockaddr_in *peer)
{
    for (int i = 0; i < MAX_SESSIONS; i++)
        if (table[i].in_use &&
            table[i].peer.sin_addr.s_addr == peer->sin_addr.s_addr &&
            table[i].peer.sin_port == peer->sin_port)
            return &table[i];
    return NULL;
}

session_t *session_create(const struct sockaddr_in *peer)
{
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!table[i].in_use) {
            memset(&table[i], 0, sizeof table[i]);
            table[i].in_use = 1;
            table[i].peer = *peer;
            table[i].fd = -1;
            table[i].last_activity = time(NULL);
            return &table[i];
        }
    }
    return NULL;
}

void session_close(session_t *s)
{
    if (s->fd >= 0) close(s->fd);
    s->fd = -1;
    s->in_use = 0;
}

void sessions_gc(time_t now)
{
    for (int i = 0; i < MAX_SESSIONS; i++)
        if (table[i].in_use && now - table[i].last_activity > SESSION_TIMEOUT_S)
            session_close(&table[i]);   /* mantém o .part: o cliente pode voltar e retomar */
}

int name_in_use(const char *name)
{
    for (int i = 0; i < MAX_SESSIONS; i++)
        if (table[i].in_use && strcmp(table[i].name, name) == 0) return 1;
    return 0;
}

int sanitize_filename(const char *in, size_t in_len, char *out, size_t out_sz)
{
    if (in_len == 0 || in_len > MAX_NAME_LEN || in_len >= out_sz) return -1;
    for (size_t i = 0; i < in_len; i++)
        if (in[i] == '/' || in[i] == '\\' || in[i] == '\0') return -1;
    if ((in_len == 1 && in[0] == '.') || (in_len == 2 && in[0] == '.' && in[1] == '.')) return -1;
    memcpy(out, in, in_len);
    out[in_len] = '\0';
    return 0;
}
