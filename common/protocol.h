/*
 * INE5418 - Computação Distribuída - T1: Transferência de Arquivos Distribuída
 * Autores: NomeA, NomeB, NomeC   <-- TODO: preencher (obrigatório na entrega)
 *
 * protocol.h - Formato das mensagens trocadas entre sender e receiver (UDP)
 *              + funções utilitárias de rede compartilhadas pelos dois.
 *
 * Formato do datagrama (todos os inteiros em ordem de rede / big-endian):
 *
 *   byte 0     1       2-3           4-11              12-15            16...
 *   +------+-------+-------------+----------------+----------------+-----------+
 *   | type | flags | payload_len |     offset     | crc32(payload) |  payload  |
 *   | 1 B  | 1 B   |    2 B      |      8 B       |      4 B       | 0..1024 B |
 *   +------+-------+-------------+----------------+----------------+-----------+
 *
 * Uso do campo "offset" em cada mensagem:
 *   START      offset = tamanho total do arquivo; payload = nome do arquivo (basename)
 *   START_ACK  offset = offset a partir do qual o sender deve (re)começar (0 = novo)
 *   DATA       offset = posição, no arquivo, do primeiro byte do payload
 *   ACK        offset = próximo offset esperado pelo receiver (= bytes já gravados)
 *   END        offset = tamanho total do arquivo
 *   END_ACK    offset = tamanho total gravado
 *   ERROR      flags = enum err_code; payload = texto opcional
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>

#define DEFAULT_PORT   9000
#define HEADER_SIZE    16
#define MAX_PAYLOAD    1024                 /* cabe em 1 pacote Ethernet sem fragmentar */
#define MAX_PKT_SIZE   (HEADER_SIZE + MAX_PAYLOAD)
#define MAX_NAME_LEN   255

#define TIMEOUT_MS     300                  /* espera pelo ACK antes de retransmitir */
#define MAX_RETRIES    20                   /* tentativas antes de desistir */

/* Limite do enunciado: no máximo 32 KiB de arquivo em memória por vez. */
#define MAX_MEM_BYTES  (32 * 1024)

enum msg_type {
    MSG_START = 1,
    MSG_START_ACK,
    MSG_DATA,
    MSG_ACK,
    MSG_END,
    MSG_END_ACK,
    MSG_ERROR
};

enum err_code {
    ERR_NONE = 0,
    ERR_BUSY,        /* já existe uma transferência ativa para esse arquivo */
    ERR_BAD_NAME,    /* nome de arquivo inválido */
    ERR_IO,          /* falha de disco no receiver */
    ERR_PROTOCOL     /* mensagem inesperada */
};

typedef struct {
    uint8_t  type;
    uint8_t  flags;
    uint16_t payload_len;
    uint64_t offset;
    uint8_t  payload[MAX_PAYLOAD];
} pkt_t;

/* ---- serialização ---- */
uint32_t    crc32_calc(const uint8_t *data, size_t len);
size_t      pkt_encode(uint8_t *buf, uint8_t type, uint8_t flags, uint64_t offset,
                       const void *payload, uint16_t len);   /* bytes escritos; 0 se erro */
int         pkt_decode(const uint8_t *buf, size_t n, pkt_t *out); /* 0 ok, -1 inválido/CRC ruim */
const char *pkt_type_name(uint8_t type);

/* ---- rede ---- */

/* Envia um pacote. Se a variável de ambiente SIM_LOSS=<0..100> estiver definida,
 * descarta esse percentual dos pacotes ANTES de enviar (simula perda na rede).
 * Retorna 0 em sucesso (mesmo se descartado de propósito), -1 em erro de sendto. */
int net_send_pkt(int fd, const struct sockaddr_in *to, uint8_t type, uint8_t flags,
                 uint64_t offset, const void *payload, uint16_t len);

/* Espera até timeout_ms por um pacote válido.
 * Retorna: 1 = pacote recebido (out/from preenchidos), 0 = timeout,
 *         -1 = erro de sistema, -2 = datagrama recebido mas inválido (descarte-o). */
int net_recv_pkt(int fd, pkt_t *out, struct sockaddr_in *from, int timeout_ms);

#endif /* PROTOCOL_H */
