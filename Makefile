CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -std=gnu11 -O2 -D_FILE_OFFSET_BITS=64 -Icommon -Ireceiver
BIN     := bin

.PHONY: all clean test

all: $(BIN)/sender $(BIN)/receiver

$(BIN):
	mkdir -p $(BIN)

$(BIN)/sender: sender/sender.c common/protocol.c common/protocol.h | $(BIN)
	$(CC) $(CFLAGS) -o $@ sender/sender.c common/protocol.c

$(BIN)/receiver: receiver/receiver.c receiver/session.c receiver/session.h common/protocol.c common/protocol.h | $(BIN)
	$(CC) $(CFLAGS) -o $@ receiver/receiver.c receiver/session.c common/protocol.c

$(BIN)/test_protocol: tests/test_protocol.c common/protocol.c common/protocol.h | $(BIN)
	$(CC) $(CFLAGS) -o $@ tests/test_protocol.c common/protocol.c

# Teste unitário do formato dos pacotes (encode/decode/CRC)
test: $(BIN)/test_protocol
	./$(BIN)/test_protocol

clean:
	rm -rf $(BIN)
