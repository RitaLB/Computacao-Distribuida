#!/usr/bin/env bash
# Transferência simples sem perdas.  Uso: tests/test_basic.sh [tamanho_MB=5]
set -euo pipefail
source "$(dirname "$0")/common.sh"

gen_file "$WORK/in/basic.bin" "${1:-5}"
start_receiver
"$ROOT/bin/sender" 127.0.0.1 "$WORK/in/basic.bin" "$PORT"
same_file "$WORK/in/basic.bin" "$WORK/out/basic.bin"
ls "$WORK/out" | grep -q '\.part' && { echo "FALHA: sobrou .part"; exit 1; } || true
