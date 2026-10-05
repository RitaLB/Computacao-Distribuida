#!/usr/bin/env python3
"""
INE5418 - T1 - Autores: NomeA, NomeB, NomeC   <-- TODO

Servidor web (somente biblioteca padrão do Python 3).

Uso: python3 webserver/server.py <diretorio> [--port 8080]

API:  GET /files  ->  lista os arquivos do diretório em JSON.

Não precisa conversar com o receiver: ele só lê o MESMO diretório onde o
receiver grava. Convenção de nomes criada pelo receiver:
    nome        -> transferência concluída   => "available"
    nome.part   -> transferência em andamento => "in_transfer"
    nome.part.meta -> metadado interno (ignorado)
"""
import argparse
import json
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

PART = ".part"
META = ".part.meta"


def list_files(directory):
    files = {}
    with os.scandir(directory) as it:
        for e in it:
            if not e.is_file(follow_symlinks=False) or e.name.endswith(META):
                continue
            if e.name.endswith(PART):
                name, status = e.name[: -len(PART)], "in_transfer"
            else:
                name, status = e.name, "available"
            try:
                size = e.stat().st_size
            except OSError:          # arquivo pode ter sido renomeado agora (fim da transferência)
                continue
            # se existir "x" e "x.part" ao mesmo tempo, "em transferência" prevalece
            if name not in files or status == "in_transfer":
                files[name] = {"name": name, "size": size, "status": status}
    return sorted(files.values(), key=lambda f: f["name"])


def make_handler(directory):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if urlparse(self.path).path.rstrip("/") == "/files":
                body = json.dumps({"files": list_files(directory)}, ensure_ascii=False).encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            else:
                self.send_error(404, "Use GET /files")

    return Handler


def main():
    ap = argparse.ArgumentParser(description="Lista arquivos do diretório de upload (GET /files)")
    ap.add_argument("directory", help="diretório monitorado (o mesmo usado pelo receiver)")
    ap.add_argument("--port", type=int, default=8080)
    args = ap.parse_args()

    if not os.path.isdir(args.directory):
        ap.error(f"diretório inválido: {args.directory}")

    srv = ThreadingHTTPServer(("0.0.0.0", args.port), make_handler(args.directory))
    print(f"Servidor web em http://0.0.0.0:{args.port}/files  (dir: {args.directory})")
    srv.serve_forever()


if __name__ == "__main__":
    main()
