/*
 * INE5418 - T1 - Autores: NomeA, NomeB, NomeC   <-- TODO
 *
 * receiver.c - Servidor de upload (UDP + Stop and Wait), clientes concorrentes.
 * Uso: ./receiver [porta] [diretorio]      (padrões: 9000 e $HOME; aceita ":9000")
 *
 * Um socket UDP + laço poll + tabela de sessões por (ip, porta) do cliente:
 * cada datagrama vai para a sessão do remetente (sem threads, sem locks).
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "protocol.h"
#include "session.h"

static void reply(int sock, const struct sockaddr_in *peer, uint8_t type, uint64_t offset)
{
    net_send_pkt(sock, peer, type, 0, offset, NULL, 0);
}

static void send_error(int sock, const struct sockaddr_in *peer, uint8_t code, const char *msg)
{
    net_send_pkt(sock, peer, MSG_ERROR, code, 0, msg, (uint16_t)strlen(msg));
}

/* START: nome + tamanho total. Cria ou RETOMA o .part e responde com o offset. */
static void handle_start(int sock, const struct sockaddr_in *peer, const pkt_t *p, const char *dir)
{
    char name[MAX_NAME_LEN + 1];
    if (sanitize_filename((const char *)p->payload, p->payload_len, name, sizeof name) < 0) {
        send_error(sock, peer, ERR_BAD_NAME, "nome de arquivo invalido");
        return;
    }

    session_t *s = session_find(peer);              /* START repetido ou porta reutilizada */
    session_t *other = session_by_name(name);       /* alguém já enviando esse nome? */
    if (other && other != s) {
        if (other->peer.sin_addr.s_addr != peer->sin_addr.s_addr) {
            send_error(sock, peer, ERR_BUSY, "arquivo em transferencia por outro cliente");
            return;
        }
        session_close(other);   /* mesmo host: é o cliente que morreu e voltou (porta nova) */
    }
    if (!s && !(s = session_create(peer))) {
        send_error(sock, peer, ERR_BUSY, "servidor cheio");
        return;
    }
    if (s->fd >= 0) { close(s->fd); s->fd = -1; }

    int n1 = snprintf(s->final_path, PATH_MAX, "%s/%s", dir, name);
    int n2 = snprintf(s->part_path, PATH_MAX, "%s/%s.part", dir, name);
    if (n1 >= PATH_MAX || n2 >= PATH_MAX) { session_close(s); send_error(sock, peer, ERR_BAD_NAME, "caminho longo"); return; }

    s->fd = open(s->part_path, O_RDWR | O_CREAT, 0644);
    struct stat st;
    if (s->fd < 0 || fstat(s->fd, &st) < 0) { session_close(s); send_error(sock, peer, ERR_IO, "erro ao abrir .part"); return; }

    s->total_size = p->offset;
    s->received = (uint64_t)st.st_size;             /* o tamanho do .part É o estado de retomada */
    if (s->received > s->total_size) {              /* .part inconsistente: recomeça */
        if (ftruncate(s->fd, 0) < 0) { session_close(s); send_error(sock, peer, ERR_IO, "ftruncate"); return; }
        s->received = 0;
    }
    snprintf(s->name, sizeof s->name, "%s", name);
    s->done = 0;
    s->dups = 0;
    s->last_activity = time(NULL);

    printf("[%s] START de %s:%d total=%llu retomando_em=%llu\n", name, inet_ntoa(peer->sin_addr),
           ntohs(peer->sin_port), (unsigned long long)s->total_size, (unsigned long long)s->received);
    reply(sock, peer, MSG_START_ACK, s->received);
}

/* DATA: grava só se offset == esperado. Duplicata/fora de ordem: NÃO grava, reenvia o ACK. */
static void handle_data(int sock, const struct sockaddr_in *peer, const pkt_t *p)
{
    session_t *s = session_find(peer);
    if (!s || s->done || s->fd < 0) { send_error(sock, peer, ERR_PROTOCOL, "sem sessao"); return; }
    s->last_activity = time(NULL);

    if (p->offset == s->received) {
        if (s->received + p->payload_len > s->total_size ||
            pwrite(s->fd, p->payload, p->payload_len, (off_t)s->received) != (ssize_t)p->payload_len) {
            send_error(sock, peer, ERR_IO, "erro ao gravar");
            return;
        }
        s->received += p->payload_len;              /* write-before-ack: só confirma depois de gravar */
    } else {
        s->dups++;
    }
    reply(sock, peer, MSG_ACK, s->received);
}

/* END: se completo, rename(.part -> nome final) e END_ACK. END repetido: reenvia END_ACK. */
static void handle_end(int sock, const struct sockaddr_in *peer)
{
    session_t *s = session_find(peer);
    if (!s) { send_error(sock, peer, ERR_PROTOCOL, "sem sessao"); return; }
    s->last_activity = time(NULL);

    if (!s->done) {
        if (s->received != s->total_size) { send_error(sock, peer, ERR_PROTOCOL, "arquivo incompleto"); return; }
        close(s->fd);
        s->fd = -1;
        if (rename(s->part_path, s->final_path) < 0) { send_error(sock, peer, ERR_IO, "rename falhou"); return; }
        s->done = 1;
        printf("[%s] CONCLUIDO (%llu bytes, %lu duplicatas ignoradas)\n", s->name,
               (unsigned long long)s->total_size, s->dups);
    }
    reply(sock, peer, MSG_END_ACK, s->total_size);   /* se este END_ACK se perder, o END repetido cai aqui */
}

static int parse_port(const char *s)
{
    if (*s == ':') s++;
    int port = atoi(s);
    return (port > 0 && port < 65536) ? port : -1;
}

int main(int argc, char **argv)
{
    int port = DEFAULT_PORT;
    const char *dir = getenv("HOME") ? getenv("HOME") : ".";

    if (argc > 3) { fprintf(stderr, "Uso: %s [porta] [diretorio]\n", argv[0]); return 1; }
    if (argc >= 2 && (port = parse_port(argv[1])) < 0) { fprintf(stderr, "Porta inválida\n"); return 1; }
    if (argc == 3) dir = argv[2];

    struct stat st;
    if (stat(dir, &st) < 0 || !S_ISDIR(st.st_mode)) { fprintf(stderr, "Diretório inválido: %s\n", dir); return 1; }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_ANY),
                                .sin_port = htons((uint16_t)port) };
    if (sock < 0 || bind(sock, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("socket/bind"); return 1; }

    setvbuf(stdout, NULL, _IOLBF, 0);
    sessions_init();
    printf("Receiver escutando na porta %d, salvando em '%s'\n", port, dir);

    for (;;) {
        pkt_t p;
        struct sockaddr_in peer;
        int r = net_recv_pkt(sock, &p, &peer, 1000);
        if (r == 1) {
            switch (p.type) {
            case MSG_START: handle_start(sock, &peer, &p, dir); break;
            case MSG_DATA:  handle_data(sock, &peer, &p);       break;
            case MSG_END:   handle_end(sock, &peer);            break;
            default:        send_error(sock, &peer, ERR_PROTOCOL, "mensagem inesperada");
            }
        } else if (r == -1) {
            perror("recv");
        }
        sessions_gc(time(NULL));
    }
}
