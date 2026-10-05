/*
 * INE5418 - T1 - Autores: NomeA, NomeB, NomeC   <-- TODO
 *
 * receiver.c - Servidor de upload (UDP + Stop and Wait), clientes concorrentes.
 *
 * Uso: ./receiver [porta] [diretorio]      (padrões: 9000 e $HOME; aceita ":9000")
 *
 * Arquitetura: UM socket UDP + laço de eventos (poll) + tabela de sessões
 * indexada por (ip, porta) do cliente. Cada datagrama é despachado para a
 * sessão do remetente, então vários clientes progridem "ao mesmo tempo" sem
 * threads nem locks.
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "protocol.h"
#include "session.h"

static void send_error(int sock, const struct sockaddr_in *peer, uint8_t code, const char *msg)
{
    net_send_pkt(sock, peer, MSG_ERROR, code, 0, msg, (uint16_t)strlen(msg));
}

/*
 * START: nome + tamanho total.
 * TODO:
 *   1. sanitize_filename(); se inválido -> ERROR(ERR_BAD_NAME)
 *   2. se name_in_use() por OUTRA sessão -> ERROR(ERR_BUSY)
 *   3. se o cliente já tem sessão (START retransmitido): reaproveitar e só reenviar START_ACK
 *   4. montar part_path/meta_path/final_path dentro de `dir`
 *   5. se existe .part E .meta com o MESMO total_size: received = tamanho do .part (retomada)
 *      senão: truncar/criar .part vazio e gravar .meta com total_size (received = 0)
 *   6. abrir o .part (open O_WRONLY|O_CREAT), responder START_ACK(offset = received)
 */
static void handle_start(int sock, const struct sockaddr_in *peer, const pkt_t *p, const char *dir)
{
    (void)sock; (void)peer; (void)p; (void)dir;
    fprintf(stderr, "TODO: handle_start\n");
}

/*
 * DATA: offset + payload.
 * TODO:
 *   - sem sessão -> ERROR(ERR_PROTOCOL)
 *   - p->offset == s->received  : pwrite(payload) no .part; received += len; ACK(received)
 *   - p->offset <  s->received  : DUPLICADO (ACK anterior se perdeu): NÃO grava; ACK(received)
 *   - p->offset >  s->received  : fora de ordem (não deve ocorrer em Stop and Wait): ACK(received)
 *   - atualizar s->last_activity
 */
static void handle_data(int sock, const struct sockaddr_in *peer, const pkt_t *p)
{
    (void)sock; (void)peer; (void)p;
    fprintf(stderr, "TODO: handle_data\n");
}

/*
 * END: o sender diz que terminou.
 * TODO:
 *   - se s->received == s->total_size: close(fd), rename(.part -> final), unlink(.meta),
 *     responder END_ACK, fechar sessão
 *   - se não bateu: ERROR(ERR_PROTOCOL) (ou ACK(received) para o sender continuar)
 *   - END duplicado (sessão já fechada e arquivo final existe): reenviar END_ACK
 *     (pense: o END_ACK pode ter se perdido!)
 */
static void handle_end(int sock, const struct sockaddr_in *peer, const pkt_t *p)
{
    (void)sock; (void)peer; (void)p;
    fprintf(stderr, "TODO: handle_end\n");
}

static int parse_port(const char *s)
{
    if (*s == ':') s++;                      /* aceita ":9000" como no enunciado */
    int port = atoi(s);
    return (port > 0 && port < 65536) ? port : -1;
}

int main(int argc, char **argv)
{
    int port = DEFAULT_PORT;
    const char *dir = getenv("HOME");
    if (!dir) dir = ".";

    if (argc > 3) {
        fprintf(stderr, "Uso: %s [porta] [diretorio]\n", argv[0]);
        return 1;
    }
    if (argc >= 2 && (port = parse_port(argv[1])) < 0) {
        fprintf(stderr, "Porta inválida: %s\n", argv[1]);
        return 1;
    }
    if (argc == 3) dir = argv[2];

    struct stat st;
    if (stat(dir, &st) < 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Diretório inválido: %s\n", dir);
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons((uint16_t)port),
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("bind"); return 1; }

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
            case MSG_END:   handle_end(sock, &peer, &p);        break;
            default:        send_error(sock, &peer, ERR_PROTOCOL, "mensagem inesperada"); break;
            }
        } else if (r == -1) {
            perror("recv");
        }
        /* r == 0 (timeout) e r == -2 (pacote inválido): nada a fazer */

        sessions_gc(time(NULL));
    }
}
