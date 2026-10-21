"""Genera metadati sintetici per il tracker locale. Nessun payload scaricabile."""
import argparse
from pathlib import Path


def bencode(value):
    if isinstance(value, int):
        return b"i" + str(value).encode("ascii") + b"e"
    if isinstance(value, bytes):
        return str(len(value)).encode("ascii") + b":" + value
    if isinstance(value, dict):
        return b"d" + b"".join(bencode(k) + bencode(value[k]) for k in sorted(value)) + b"e"
    raise TypeError(type(value))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("path", type=Path)
    parser.add_argument("--port", type=int, default=18765)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("Porta non valida")
    info = {b"length": 32 * 1024 * 1024, b"name": b"TorrentDesk - demo locale",
            b"piece length": 1024 * 1024, b"pieces": b"x" * (20 * 32)}
    torrent = {b"announce": f"http://127.0.0.1:{args.port}/announce".encode("ascii"), b"info": info}
    args.path.parent.mkdir(parents=True, exist_ok=True)
    args.path.write_bytes(bencode(torrent))
    print(args.path)


if __name__ == "__main__":
    main()
