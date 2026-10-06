#!/usr/bin/env bash
# Tamanhos de borda: 0, 1, 1023, 1024, 1025, 2048 e 1 MiB+7 bytes.
set -euo pipefail
source "$(dirname "$0")/common.sh"
start_receiver
rc=0
for n in 0 1 1023 1024 1025 2048 1048583; do
    head -c "$n" /dev/urandom > "$WORK/in/s$n.bin"
    "$ROOT/bin/sender" 127.0.0.1 "$WORK/in/s$n.bin" "$PORT" > /dev/null
    same_file "$WORK/in/s$n.bin" "$WORK/out/s$n.bin" || rc=1
done
exit $rc
