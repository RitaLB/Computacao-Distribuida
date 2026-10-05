#!/usr/bin/env bash
# Transferência com perda simulada de pacotes (DATA e ACK).  Uso: LOSS=20 tests/test_loss.sh [MB=2]
set -euo pipefail
source "$(dirname "$0")/common.sh"
export SIM_LOSS="${LOSS:-20}"

gen_file "$WORK/in/loss.bin" "${1:-2}"
start_receiver
"$ROOT/bin/sender" 127.0.0.1 "$WORK/in/loss.bin" "$PORT"
same_file "$WORK/in/loss.bin" "$WORK/out/loss.bin"
