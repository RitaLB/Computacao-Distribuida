/*
 * INE5418 - T1 - Autores: NomeA, NomeB, NomeC   <-- TODO
 *
 * sender.c - Cliente de upload (UDP + Stop and Wait)
 *
 * Uso: ./sender <ip_do_servidor> <arquivo> [porta]
 *
 * Fluxo planejado:
 *   1) HANDSHAKE : envia START(nome, tamanho) e espera START_ACK(offset de retomada)
 *   2) DADOS     : a partir do offset, lê <= MAX_PAYLOAD bytes do arquivo (pread),
 *                  envia DATA(offset) e espera ACK(próximo offset) -> Stop and Wait
 *   3) FIM       : envia END e espera END_ACK
 *
 * Retomada após falha: não há estado local; basta rodar o comando de novo.
 * O offset de partida vem do START_ACK do receiver.
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

/*
 * Núcleo do Stop and Wait: envia um pacote e espera uma resposta do tipo
 * `expect`, retransmitindo em caso de timeout.
 *
 * TODO:
 *   for (tentativa = 0; tentativa < MAX_RETRIES; tentativa++) {
 *       net_send_pkt(...);
 *       r = net_recv_pkt(fd, in, NULL, TIMEOUT_MS);
 *       r == 0  -> timeout: retransmite (próxima iteração)
 *       r == -2 -> pacote inválido: ignora e continua esperando
 *       r == 1  -> se in->type == MSG_ERROR: aborta com mensagem;
 *                  se in->type == expect (e offset coerente): retorna 0;
 *                  senão (ACK duplicado/atrasado): ignora
 *   }
 *   retorna -1 se esgotou as tentativas
 */
static int send_and_wait(int fd, const struct sockaddr_in *srv,
                         uint8_t type, uint64_t offset, const void *payload, uint16_t len,
                         uint8_t expect, pkt_t *in)
{
    (void)fd; (void)srv; (void)type; (void)offset; (void)payload; (void)len;
    (void)expect; (void)in;
    fprintf(stderr, "TODO: send_and_wait\n");
    return -1;
}

/* TODO: manda START e devolve em *resume_offset o offset recebido no START_ACK. */
static int do_handshake(int fd, const struct sockaddr_in *srv, const char *name,
                        uint64_t total_size, uint64_t *resume_offset)
{
    (void)fd; (void)srv; (void)name; (void)total_size;
    *resume_offset = 0;
    (void)send_and_wait;
    fprintf(stderr, "TODO: do_handshake\n");
    return -1;
}

/*
 * TODO: loop de envio. Usar pread(file_fd, buf, MAX_PAYLOAD, offset)
 * (buffer pequeno => respeita o limite de 32 KiB em memória).
 * Cada DATA só avança quando chegar ACK com offset == offset_enviado + bytes_lidos.
 * Imprimir progresso (% e bytes) de vez em quando.
 */
static int send_file(int fd, const struct sockaddr_in *srv, int file_fd,
                     uint64_t total_size, uint64_t start_offset)
{
    (void)fd; (void)srv; (void)file_fd; (void)total_size; (void)start_offset;
    fprintf(stderr, "TODO: send_file\n");
    return -1;
}

/* TODO: manda END e espera END_ACK. */
static int do_finish(int fd, const struct sockaddr_in *srv, uint64_t total_size)
{
    (void)fd; (void)srv; (void)total_size;
    fprintf(stderr, "TODO: do_finish\n");
    return -1;
}

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Uso: %s <ip_do_servidor> <arquivo> [porta]\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1];
    const char *path = argv[2];
    int port = (argc == 4) ? atoi(argv[3]) : DEFAULT_PORT;

    int file_fd = open(path, O_RDONLY);
    if (file_fd < 0) { perror("open"); return 1; }

    struct stat st;
    if (fstat(file_fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        fprintf(stderr, "'%s' não é um arquivo regular\n", path);
        return 1;
    }
    uint64_t total_size = (uint64_t)st.st_size;

    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;              /* só o nome, nunca o caminho */
    if (strlen(base) == 0 || strlen(base) > MAX_NAME_LEN) {
        fprintf(stderr, "Nome de arquivo inválido\n");
        return 1;
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    struct sockaddr_in srv = { .sin_family = AF_INET, .sin_port = htons((uint16_t)port) };
    if (inet_pton(AF_INET, ip, &srv.sin_addr) != 1) {
        fprintf(stderr, "IP inválido: %s\n", ip);
        return 1;
    }

    printf("Enviando '%s' (%llu bytes) para %s:%d\n", base,
           (unsigned long long)total_size, ip, port);

    uint64_t resume = 0;
    if (do_handshake(fd, &srv, base, total_size, &resume) < 0) return 2;
    if (resume > 0) printf("Retomando a partir do byte %llu\n", (unsigned long long)resume);
    if (send_file(fd, &srv, file_fd, total_size, resume) < 0) return 3;
    if (do_finish(fd, &srv, total_size) < 0) return 4;

    printf("Transferência concluída.\n");
    close(file_fd);
    close(fd);
    return 0;
}
