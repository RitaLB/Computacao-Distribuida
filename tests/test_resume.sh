#!/usr/bin/env bash
# Mata o sender no meio (kill -9) e roda de novo: deve retomar sem duplicar/corromper.
# Uso: tests/test_resume.sh [MB=100] [segundos_ate_matar=2]
set -euo pipefail
source "$(dirname "$0")/common.sh"

gen_file "$WORK/in/resume.bin" "${1:-100}"
start_receiver

"$ROOT/bin/sender" 127.0.0.1 "$WORK/in/resume.bin" "$PORT" &
SPID=$!
sleep "${2:-2}"
kill -9 "$SPID" 2>/dev/null || true
wait "$SPID" 2>/dev/null || true
echo ">> sender morto. Estado no servidor:"; ls -l "$WORK/out"

echo ">> rodando o sender novamente (deve retomar)..."
"$ROOT/bin/sender" 127.0.0.1 "$WORK/in/resume.bin" "$PORT"

same_file "$WORK/in/resume.bin" "$WORK/out/resume.bin"
[ "$(ls "$WORK/out" | wc -l)" -eq 1 ] || { echo "FALHA: arquivos extras no destino:"; ls "$WORK/out"; exit 1; }
