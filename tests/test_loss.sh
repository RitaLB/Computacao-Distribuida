#!/usr/bin/env bash
# Perda simulada de pacotes (DATA, ACK, START, END) nos DOIS lados.
# Uso: LOSS=20 tests/test_loss.sh [KB=100]
set -euo pipefail
source "$(dirname "$0")/common.sh"
export SIM_LOSS="${LOSS:-20}"

head -c "$(( ${1:-100} * 1024 ))" /dev/urandom > "$WORK/in/loss.bin"
start_receiver
"$ROOT/bin/sender" 127.0.0.1 "$WORK/in/loss.bin" "$PORT"
same_file "$WORK/in/loss.bin" "$WORK/out/loss.bin"
grep CONCLUIDO "$WORK/receiver.log"
