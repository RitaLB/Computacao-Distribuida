# INE5418 – T1: Transferência de Arquivos Distribuída

**Grupo 1** — NomeA, NomeB, NomeC   <!-- TODO: preencher; nomes ausentes não são considerados na avaliação -->

Serviço de backup de arquivos com upload confiável sobre **UDP + Stop and Wait**, com retomada
após falha do cliente e suporte a clientes concorrentes. Três programas independentes:

| Programa | Linguagem | O que faz |
|---|---|---|
| `sender` | C (sockets Berkeley) | Envia um arquivo para o servidor de upload |
| `receiver` | C (sockets Berkeley) | Recebe arquivos de vários clientes e grava em um diretório |
| `webserver/server.py` | Python 3 (stdlib) | `GET /files`: lista arquivos e status em JSON |

## Compilação

Requisitos: `gcc`, `make` (Linux) e Python 3.8+ para o servidor web.

```bash
make          # gera bin/sender e bin/receiver
make test     # teste unitário do formato dos pacotes
make clean
```

## Execução

**Servidor de upload** (porta e diretório são opcionais; padrões `9000` e `$HOME`):
```bash
./bin/receiver [porta] [diretorio]
./bin/receiver 9000 /tmp/backup          # exemplo   (também aceita ":9000")
```

**Cliente**:
```bash
./bin/sender <ip_do_servidor> <arquivo> [porta]
./bin/sender 127.0.0.1 /path/to/file.txt # exemplo
```
Se o cliente for interrompido, **basta executar o mesmo comando novamente**: a transferência continua de onde parou.

**Servidor web** (use o *mesmo* diretório do receiver):
```bash
python3 webserver/server.py /tmp/backup --port 8080
curl http://localhost:8080/files
```
```json
{"files":[{"name":"a.txt","size":1234,"status":"available"},
          {"name":"b.iso","size":52428800,"status":"in_transfer"}]}
```

## Protocolo (resumo)

Cabeçalho de 16 bytes (tipo, flags, tamanho do payload, **offset de 64 bits**, CRC32) + até 1024 bytes de payload.
Detalhes em `common/protocol.h`.

```
sender                         receiver
  | -- START(nome, tamanho) -->  |  cria/retoma <nome>.part
  | <-- START_ACK(offset) ------ |  offset = bytes já gravados (0 se novo)
  | -- DATA(offset, bytes) ---->  |  grava e avança
  | <-- ACK(próximo offset) ---- |
  |          ...                 |
  | -- END -------------------->  |  rename(<nome>.part -> <nome>)
  | <-- END_ACK ---------------- |
```

## Decisões de projeto

- **Stop and Wait com offset como número de sequência**: um pacote por vez; timeout de 100 ms e até 20 tentativas por pacote.
- **Memória**: o sender lê no máximo 1024 bytes por vez (`pread`) — bem abaixo do limite de 32 KiB.
- **Duplicatas**: se o ACK se perde, o sender retransmite; o receiver reconhece `offset < esperado`, **não grava de novo** e reenvia o ACK.
- **Sem arquivo corrompido/duplicado**: dados vão para `<nome>.part`; só vira `<nome>` por `rename()` ao final.
- **Retomada**: o offset de retomada é o tamanho do `.part` (gravação sequencial e ACK só depois do `write`). Se o `.part` for maior que o tamanho informado, recomeça do zero. O cliente que morre e volta ganha outra porta; um START do mesmo IP e mesmo nome *assume* a sessão órfã.
- **Fim confiável**: após o `rename`, a sessão fica marcada `done`; se o `END_ACK` se perder, o `END` repetido é reconhecido.
- **Concorrência**: um socket UDP, laço `poll` e tabela de sessões por `(ip, porta)`. Mesmo nome simultâneo → `ERROR(ERR_BUSY)`.
- **Segurança básica**: o receiver aceita só o *basename* (rejeita `/`, `..`).
- **Servidor web**: não se comunica com o receiver; lê o diretório (`*.part` = em transferência).

## Testes

```bash
make
tests/test_basic.sh             # transferência simples
LOSS=20 tests/test_loss.sh      # 20% de perda simulada nos dois lados (variável SIM_LOSS)
tests/test_sizes.sh             # tamanhos de borda (0, 1, 1023, 1024, 1025 ... bytes)
tests/test_resume.sh 100 2      # mata o sender com kill -9 e retoma
tests/test_concurrent.sh        # 3 clientes simultâneos
tests/test_web.sh <diretorio>   # GET /files
```
`SIM_LOSS=<0..100>` (e `SIM_LOSS_VERBOSE=1`) faz `sender`/`receiver` descartarem pacotes de propósito.
Arquivo de 1 GB: `dd if=/dev/urandom of=1g.bin bs=1M count=1024` e compare com `sha256sum`.

## Resultados medidos (loopback)

- 1 GiB: **34 s**, hash idêntico, sender com **~1,8 MB** de memória residente (pico).
- 20 MB sem perdas: ~1,2 s; 3 clientes simultâneos de 20 MB: todos idênticos.
- 20% de perda nos dois lados: arquivo idêntico em 5/5 rodadas.
- `kill -9` do sender com ~30% enviado: retoma do byte exato e conclui idêntico, sem arquivos extras.

## Restrições do enunciado (checklist)

- [ ] Apenas sockets Berkeley, sem bibliotecas de transferência
- [ ] Nenhum `system()`, `popen()`, `exec*()`
- [ ] Arquivos de até 1 GB, no máximo 32 KiB de arquivo em memória por vez
- [ ] Retomada após interrupção do cliente sem duplicar/corromper
- [ ] Nomes dos membros no código e no relatório
- [ ] Entrega: `Grupo1-NomeA-NomeB.zip` (código + README + relatório .pdf)
