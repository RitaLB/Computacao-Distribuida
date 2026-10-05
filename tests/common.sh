# Funções compartilhadas pelos testes. Use: source "$(dirname "$0")/common.sh"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${PORT:-9000}"
WORK="$(mktemp -d)"
RPID=""
mkdir -p "$WORK/in" "$WORK/out"

cleanup() { [ -n "$RPID" ] && kill "$RPID" 2>/dev/null || true; rm -rf "$WORK"; }
trap cleanup EXIT

[ -x "$ROOT/bin/sender" ] && [ -x "$ROOT/bin/receiver" ] || { echo "Rode 'make' primeiro"; exit 1; }

gen_file() { dd if=/dev/urandom of="$1" bs=1M count="$2" status=none; }   # gen_file <caminho> <MB>

start_receiver() {
    "$ROOT/bin/receiver" "$PORT" "$WORK/out" > "$WORK/receiver.log" 2>&1 &
    RPID=$!
    sleep 0.5
}

same_file() {   # same_file <original> <recebido>
    if cmp -s "$1" "$2"; then echo "OK   : $(basename "$1") idêntico"; else echo "FALHA: $(basename "$1") difere ou não existe"; return 1; fi
}
