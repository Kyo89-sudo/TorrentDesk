"""Tracker HTTP didattico: solo loopback, nessun peer reale."""
import argparse
import gzip
import json
import socket
import struct
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit, parse_qs


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=18765)
    parser.add_argument("--fast", action="store_true", help="Intervallo di 1 s per i test automatici")
    args = parser.parse_args()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            url = urlsplit(self.path)
            if url.path == "/health":
                body = b"ok"
            elif url.path == "/redirect":
                self.send_response(302)
                self.send_header("Location", "/announce")
                self.end_headers()
                return
            elif url.path == "/fail":
                self.send_error(503)
                return
            elif url.path == "/announce":
                query = parse_qs(url.query)
                print(json.dumps({"event": query.get("event", ["periodic"])[0],
                                  "uploaded": query.get("uploaded", ["0"])[0],
                                  "query": url.query,
                                  "headers": {k: v for k, v in self.headers.items()}}),
                      flush=True)
                interval = 1 if args.fast else 30
                body = (b"d8:completei2e10:incompletei3e8:intervali" +
                        str(interval).encode("ascii") + b"ee")
                body = gzip.compress(body)
            else:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            if url.path == "/announce":
                self.send_header("Content-Encoding", "gzip")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    def udp_tracker():
        """BEP 15 on the same port: connect + announce, loopback only."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("127.0.0.1", args.port))
        connections = set()
        while True:
            data, addr = sock.recvfrom(2048)
            if len(data) >= 16 and data[:8] == struct.pack(">Q", 0x41727101980) and \
                    struct.unpack(">I", data[8:12])[0] == 0:
                cid = struct.unpack(">Q", data[12:16] + data[12:16])[0]
                connections.add(cid)
                sock.sendto(struct.pack(">IIQ", 0, struct.unpack(">I", data[12:16])[0], cid), addr)
            elif len(data) >= 98 and struct.unpack(">I", data[8:12])[0] == 1:
                cid, _, tx = struct.unpack(">QII", data[:16])
                if cid not in connections:
                    sock.sendto(struct.pack(">II", 3, tx) + b"unknown connection", addr)
                    continue
                peer = data[36:56]
                downloaded, left, uploaded, event, ip, key, numwant, port = \
                    struct.unpack(">QQQIIIiH", data[56:98])
                print(json.dumps({"protocol": "udp",
                                  "event": {0: "periodic", 1: "completed", 2: "started",
                                            3: "stopped"}.get(event, "?"),
                                  "uploaded": str(uploaded), "left": str(left),
                                  "peer_id": peer.hex(), "key": key, "numwant": numwant,
                                  "port": port}), flush=True)
                interval = 1 if args.fast else 30
                sock.sendto(struct.pack(">IIIII", 1, tx, interval, 3, 2), addr)

    threading.Thread(target=udp_tracker, daemon=True).start()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"Local tracker: http://127.0.0.1:{args.port}/announce", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
