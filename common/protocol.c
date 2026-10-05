/*
 * INE5418 - T1 - Autores: NomeA, NomeB, NomeC   <-- TODO
 * protocol.c - implementação de protocol.h
 */
#include "protocol.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* ---------- helpers big-endian ---------- */
static void put16(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = v; }
static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = v >> (24 - 8 * i); }
static void put64(uint8_t *p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = v >> (56 - 8 * i); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t get32(const uint8_t *p) { uint32_t v = 0; for (int i = 0; i < 4; i++) v = v << 8 | p[i]; return v; }
static uint64_t get64(const uint8_t *p) { uint64_t v = 0; for (int i = 0; i < 8; i++) v = v << 8 | p[i]; return v; }

/* ---------- CRC32 (polinômio 0xEDB88320, sem biblioteca externa) ---------- */
uint32_t crc32_calc(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++)
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    return ~crc;
}

/* ---------- serialização ---------- */
size_t pkt_encode(uint8_t *buf, uint8_t type, uint8_t flags, uint64_t offset,
                  const void *payload, uint16_t len)
{
    if (len > MAX_PAYLOAD) return 0;
    buf[0] = type;
    buf[1] = flags;
    put16(buf + 2, len);
    put64(buf + 4, offset);
    put32(buf + 12, len ? crc32_calc(payload, len) : 0);
    if (len) memcpy(buf + HEADER_SIZE, payload, len);
    return HEADER_SIZE + len;
}

int pkt_decode(const uint8_t *buf, size_t n, pkt_t *out)
{
    if (n < HEADER_SIZE) return -1;
    uint16_t len = get16(buf + 2);
    if (len > MAX_PAYLOAD || n != (size_t)HEADER_SIZE + len) return -1;
    if (buf[0] < MSG_START || buf[0] > MSG_ERROR) return -1;
    if (get32(buf + 12) != (len ? crc32_calc(buf + HEADER_SIZE, len) : 0)) return -1;

    out->type = buf[0];
    out->flags = buf[1];
    out->payload_len = len;
    out->offset = get64(buf + 4);
    if (len) memcpy(out->payload, buf + HEADER_SIZE, len);
    return 0;
}

const char *pkt_type_name(uint8_t t)
{
    switch (t) {
    case MSG_START:     return "START";
    case MSG_START_ACK: return "START_ACK";
    case MSG_DATA:      return "DATA";
    case MSG_ACK:       return "ACK";
    case MSG_END:       return "END";
    case MSG_END_ACK:   return "END_ACK";
    case MSG_ERROR:     return "ERROR";
    default:            return "?";
    }
}

/* ---------- simulação de perda (para testes e para a defesa) ---------- */
static int should_drop(void)
{
    static int init = 0, percent = 0;
    if (!init) {
        const char *e = getenv("SIM_LOSS");
        percent = e ? atoi(e) : 0;
        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;
        srand((unsigned)time(NULL) ^ (unsigned)getpid());
        init = 1;
    }
    return percent > 0 && (rand() % 100) < percent;
}

/* ---------- rede ---------- */
int net_send_pkt(int fd, const struct sockaddr_in *to, uint8_t type, uint8_t flags,
                 uint64_t offset, const void *payload, uint16_t len)
{
    uint8_t buf[MAX_PKT_SIZE];
    size_t n = pkt_encode(buf, type, flags, offset, payload, len);
    if (n == 0) return -1;

    if (should_drop()) {
        if (getenv("SIM_LOSS_VERBOSE"))
            fprintf(stderr, "[SIM_LOSS] descartado %s offset=%llu\n",
                    pkt_type_name(type), (unsigned long long)offset);
        return 0;
    }
    return sendto(fd, buf, n, 0, (const struct sockaddr *)to, sizeof *to) < 0 ? -1 : 0;
}

int net_recv_pkt(int fd, pkt_t *out, struct sockaddr_in *from, int timeout_ms)
{
    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int r = poll(&pfd, 1, timeout_ms);
    if (r == 0) return 0;
    if (r < 0) return errno == EINTR ? 0 : -1;

    uint8_t buf[MAX_PKT_SIZE + 1];          /* +1 para detectar datagrama grande demais */
    struct sockaddr_in src;
    socklen_t sl = sizeof src;
    ssize_t n = recvfrom(fd, buf, sizeof buf, 0, (struct sockaddr *)&src, &sl);
    if (n < 0) return -1;
    if (from) *from = src;
    return pkt_decode(buf, (size_t)n, out) == 0 ? 1 : -2;
}
