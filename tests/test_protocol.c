/* INE5418 - T1 - Teste unitário do protocolo: encode -> decode e detecção de corrupção. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "protocol.h"

int main(void)
{
    uint8_t buf[MAX_PKT_SIZE];
    pkt_t p;
    const char *msg = "hello udp";

    size_t n = pkt_encode(buf, MSG_DATA, 0, 0x1122334455667788ULL, msg, (uint16_t)strlen(msg));
    assert(n == HEADER_SIZE + strlen(msg));
    assert(pkt_decode(buf, n, &p) == 0);
    assert(p.type == MSG_DATA && p.offset == 0x1122334455667788ULL);
    assert(p.payload_len == strlen(msg) && memcmp(p.payload, msg, p.payload_len) == 0);

    buf[HEADER_SIZE] ^= 0xFF;                         /* corrompe 1 byte do payload */
    assert(pkt_decode(buf, n, &p) == -1);

    n = pkt_encode(buf, MSG_ACK, 0, 42, NULL, 0);     /* sem payload */
    assert(n == HEADER_SIZE && pkt_decode(buf, n, &p) == 0 && p.offset == 42);

    assert(pkt_decode(buf, 5, &p) == -1);             /* curto demais */
    assert(crc32_calc((const uint8_t *)"123456789", 9) == 0xCBF43926u);  /* valor de referência */

    puts("test_protocol: OK");
    return 0;
}
