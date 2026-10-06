/*
 * INE5418 - T1 - Autores: NomeA, NomeB, NomeC   <-- TODO
 *
 * sender.c - Cliente de upload (UDP + Stop and Wait).
 * Uso: ./sender <ip_do_servidor> <arquivo> [porta]
 *
 * Fluxo: START(nome,tamanho) -> START_ACK(offset de retomada)
 *        DATA(offset,bytes)  -> ACK(próximo offset)   (repetido; um pacote por vez)
 *        END                 -> END_ACK
 * O sender não guarda estado: para retomar basta rodar o comando de novo.
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "protocol.h"

#define WANT_ANY UINT64_MAX

static unsigned long retrans = 0;      /* retransmissões (evidência p/ relatório) */

static long now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

/*
 * Núcleo do Stop and Wait: envia e espera resposta do tipo `expect` (com offset == want,
 * ou qualquer offset se want == WANT_ANY). No timeout, retransmite (até MAX_RETRIES).
 * Respostas antigas/duplicadas são ignoradas SEM retransmitir (evita multiplicar
 * pacotes: o "sorcerer's apprentice bug" do Stop and Wait).
 */
static int send_and_wait(int fd, const struct sockaddr_in *srv, uint8_t type, uint64_t offset,
                         const void *payload, uint16_t len, uint8_t expect, uint64_t want,
                         pkt_t *in)
{
    for (int tries = 0; tries < MAX_RETRIES; tries++) {
        if (tries > 0) retrans++;
        if (net_send_pkt(fd, srv, type, 0, offset, payload, len) < 0) { perror("sendto"); return -1; }

        long deadline = now_ms() + TIMEOUT_MS, left;
        while ((left = deadline - now_ms()) > 0) {
            int r = net_recv_pkt(fd, in, NULL, (int)left);
            if (r == -1) { perror("recv"); return -1; }
            if (r != 1) continue;                      /* timeout (o while termina) ou pacote inválido */
            if (in->type == MSG_ERROR) {
                fprintf(stderr, "Erro do servidor: %.*s\n", in->payload_len, (char *)in->payload);
                return -1;
            }
            if (in->type == expect && (want == WANT_ANY || in->offset == want)) return 0;
        }
    }
    fprintf(stderr, "Sem resposta do servidor após %d tentativas\n", MAX_RETRIES);
    return -1;
}

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Uso: %s <ip_do_servidor> <arquivo> [porta]\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1], *path = argv[2];
    int port = (argc == 4) ? atoi(argv[3]) : DEFAULT_PORT;

    int file_fd = open(path, O_RDONLY);
    struct stat st;
    if (file_fd < 0 || fstat(file_fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        fprintf(stderr, "Não foi possível abrir '%s' como arquivo regular\n", path);
        return 1;
    }
    uint64_t total = (uint64_t)st.st_size;

    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;                     /* só o nome, nunca o caminho */
    if (*name == '\0' || strlen(name) > MAX_NAME_LEN) { fprintf(stderr, "Nome inválido\n"); return 1; }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in srv = { .sin_family = AF_INET, .sin_port = htons((uint16_t)port) };
    if (fd < 0 || inet_pton(AF_INET, ip, &srv.sin_addr) != 1) { fprintf(stderr, "IP/socket inválido\n"); return 1; }

    printf("Enviando '%s' (%llu bytes) para %s:%d\n", name, (unsigned long long)total, ip, port);

    /* 1) handshake: o servidor informa quantos bytes já tem (retomada) */
    pkt_t in;
    if (send_and_wait(fd, &srv, MSG_START, total, name, (uint16_t)strlen(name),
                      MSG_START_ACK, WANT_ANY, &in) < 0) return 2;
    uint64_t off = in.offset;
    if (off > total) { fprintf(stderr, "Offset de retomada inválido\n"); return 2; }
    if (off > 0) printf("Retomando a partir do byte %llu\n", (unsigned long long)off);

    /* 2) dados: no máximo MAX_PAYLOAD bytes do arquivo em memória por vez (<< 32 KiB) */
    uint8_t buf[MAX_PAYLOAD];
    int last_pct = -1;
    while (off < total) {
        uint64_t left = total - off;
        ssize_t n = pread(file_fd, buf, left < MAX_PAYLOAD ? left : MAX_PAYLOAD, (off_t)off);
        if (n <= 0) { perror("pread"); return 3; }
        if (send_and_wait(fd, &srv, MSG_DATA, off, buf, (uint16_t)n, MSG_ACK, off + (uint64_t)n, &in) < 0)
            return 3;
        off += (uint64_t)n;
        int pct = (int)(off * 100 / total);
        if (pct != last_pct) { last_pct = pct; printf("\r%3d%%", pct); fflush(stdout); }
    }

    /* 3) fim */
    if (send_and_wait(fd, &srv, MSG_END, total, NULL, 0, MSG_END_ACK, WANT_ANY, &in) < 0) return 4;
    printf("\nTransferência concluída (%lu retransmissões).\n", retrans);
    close(file_fd);
    close(fd);
    return 0;
}
