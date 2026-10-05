#!/usr/bin/env bash
# Sobe o servidor web e consulta GET /files.  Uso: tests/test_web.sh <diretorio>
set -euo pipefail
DIR="${1:?informe o diretório}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
python3 "$ROOT/webserver/server.py" "$DIR" --port 8080 & SPID=$!
trap 'kill $SPID 2>/dev/null || true' EXIT
sleep 0.5
curl -s http://127.0.0.1:8080/files | python3 -m json.tool
