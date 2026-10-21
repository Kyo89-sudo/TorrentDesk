"""Tracker HTTP didattico: solo loopback, nessun peer reale."""
import argparse
import gzip
import json
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
                                  "uploaded": query.get("uploaded", ["0"])[0]}), flush=True)
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
