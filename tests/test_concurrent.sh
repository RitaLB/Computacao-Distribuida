#!/usr/bin/env bash
# Três clientes enviando arquivos diferentes ao mesmo tempo.  Uso: tests/test_concurrent.sh [MB=20]
set -euo pipefail
source "$(dirname "$0")/common.sh"

for n in a b c; do gen_file "$WORK/in/$n.bin" "${1:-20}"; done
start_receiver

pids=()
for n in a b c; do "$ROOT/bin/sender" 127.0.0.1 "$WORK/in/$n.bin" "$PORT" > "$WORK/sender_$n.log" 2>&1 & pids+=($!); done
for p in "${pids[@]}"; do wait "$p"; done

rc=0
for n in a b c; do same_file "$WORK/in/$n.bin" "$WORK/out/$n.bin" || rc=1; done
exit $rc
