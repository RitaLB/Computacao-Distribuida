# INE5418 – Relatório do T1: Transferência de Arquivos Distribuída

**Grupo 1:** NomeA, NomeB, NomeC   <!-- TODO -->

> Este arquivo é um rascunho. O relatório entregue deve ser um **.pdf**
> (ex.: `pandoc relatorio.md -o relatorio.pdf`, ou escrever no Google Docs/LaTeX e exportar).
> Os diagramas abaixo estão em Mermaid (renderizam no GitHub/VS Code); exporte como imagem para o PDF.

## 1. Retransmissão e garantia de entrega

**Como a implementação lida com retransmissão e garantia de entrega?**

TODO: explicar
- Stop and Wait: um DATA em voo; só avança com ACK correspondente
- Timeout (300 ms) e limite de tentativas (20)
- Perda do DATA vs perda do ACK
- Idempotência no receiver: `offset < esperado` → não grava, só reenvia ACK
- CRC32 descarta pacotes corrompidos (tratados como perdidos)
- Gravação em `.part` + `rename()` atômico ao fim

### Caso de uso: falha do processo cliente

TODO: ajustar números reais após os testes.

```mermaid
sequenceDiagram
    participant S as sender
    participant R as receiver
    S->>R: START(nome=a.bin, tamanho=100MB)
    R-->>S: START_ACK(offset=0)
    S->>R: DATA(offset=0)
    R-->>S: ACK(1024)
    Note over S: ... transferindo ...
    S->>R: DATA(offset=40960)
    R-->>S: ACK(41984)
    Note over S: ✖ sender morre (kill -9)
    Note over R: a.bin.part = 41984 bytes (estado persistido)
    Note over S: usuário executa o sender novamente
    S->>R: START(nome=a.bin, tamanho=100MB)
    R-->>S: START_ACK(offset=41984)
    S->>R: DATA(offset=41984)
    R-->>S: ACK(43008)
    Note over S,R: ... continua ...
    S->>R: END
    Note over R: rename(a.bin.part -> a.bin)
    R-->>S: END_ACK
```

## 2. Clientes concorrentes

**Como a implementação lida com clientes concorrentes?**

TODO: explicar
- Um socket UDP + `poll` + tabela de sessões por (ip, porta)
- Cada sessão tem seu próprio `.part`, offset e fd
- Mesmo nome ao mesmo tempo → `ERR_BUSY`
- Timeout de sessão inativa (o `.part` é mantido)

### Caso de uso

```mermaid
sequenceDiagram
    participant A as cliente A
    participant B as cliente B
    participant R as receiver
    A->>R: START(x.bin)
    B->>R: START(y.bin)
    R-->>A: START_ACK(0)
    R-->>B: START_ACK(0)
    A->>R: DATA(x, 0)
    B->>R: DATA(y, 0)
    R-->>B: ACK(1024)
    R-->>A: ACK(1024)
    Note over R: sessões independentes: x.bin.part e y.bin.part
```

## 3. E se pudéssemos usar TCP?

TODO: discutir o que simplificaria
- Entrega confiável, ordenada e sem duplicatas pelo próprio protocolo
- Sem timeout/retransmissão/numeração de sequência manual
- Controle de fluxo e de congestionamento prontos
- Throughput bem maior (janela deslizante vs. Stop and Wait)
- O que ainda precisaria ser feito: retomada (offset), protocolo de aplicação, concorrência

## Referências
- Stop-and-wait ARQ — Wikipedia / GeeksforGeeks (links do enunciado)
- Material da disciplina: INE5418_05_sockets, INE5418_09_multiplex_sockets
