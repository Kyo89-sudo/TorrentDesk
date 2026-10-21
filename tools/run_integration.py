"""Avvia un tracker locale isolato, esegue i test nativi e lo arresta."""
import json
import subprocess
from urllib.parse import unquote_to_bytes
import sys
import time
import tempfile
from pathlib import Path
from urllib.request import urlopen


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Uso: run_integration.py PERCORSO_TEST_EXE CARTELLA_SORGENTI")
    executable = Path(sys.argv[1]).resolve()
    source = Path(sys.argv[2]).resolve()
    with tempfile.TemporaryFile(mode="w+b") as log:
        server = subprocess.Popen([sys.executable, str(source / "tools/local_tracker.py"),
                                   "--fast"], stdout=log, stderr=log)
        try:
            ready = False
            for _ in range(50):
                if server.poll() is not None:
                    raise RuntimeError("Il tracker di test non si è avviato. Controllare la porta 18765.")
                try:
                    with urlopen("http://127.0.0.1:18765/health", timeout=0.2) as response:
                        ready = response.read() == b"ok"
                    if ready:
                        break
                except OSError:
                    time.sleep(0.1)
            if not ready:
                raise RuntimeError("Timeout tracker locale")
            result = subprocess.run([str(executable), str(source), "--network"], timeout=45)
            if result.returncode:
                raise SystemExit(result.returncode)
            log.seek(0)
            events = log.read().decode("utf-8", errors="replace")
            for event in ('"started"', '"periodic"', '"stopped"'):
                if event not in events:
                    raise RuntimeError(f"Evento non osservato: {event}")
            print("PASS: tracker locale ha osservato started, periodic e stopped")
            check_rtorrent(events)
            check_udp(events)
        finally:
            server.terminate()
            server.wait(timeout=5)


def check_udp(log_text):
    """Il tracker UDP locale deve aver ricevuto started, periodic e stopped validi."""
    seen = {}
    for line in log_text.splitlines():
        try:
            item = json.loads(line)
        except ValueError:
            continue
        if item.get("protocol") == "udp":
            if item["left"] != "0" or len(bytes.fromhex(item["peer_id"])) != 20:
                raise RuntimeError(f"Annuncio UDP non valido: {item}")
            seen.setdefault(item["event"], 0)
            seen[item["event"]] += 1
    for event in ("started", "periodic", "stopped"):
        if not seen.get(event):
            raise RuntimeError(f"Evento UDP non osservato: {event} ({seen})")
    print(f"PASS: tracker UDP locale (BEP 15) ha osservato {seen}")


def check_rtorrent(log_text):
    """Confronta le richieste rTorrent registrate dal tracker con il profilo."""
    requests = []
    for line in log_text.splitlines():
        try:
            item = json.loads(line)
        except ValueError:
            continue
        if item.get("headers", {}).get("User-Agent", "").startswith("rtorrent/"):
            requests.append(item)
    if len(requests) < 5:
        raise RuntimeError(f"Annunci rTorrent insufficienti: {len(requests)}")
    order = ["info_hash", "peer_id", "key", "compact", "port", "uploaded", "downloaded", "left"]
    peers, keys = set(), set()
    for item in requests:
        h = item["headers"]
        if h.get("User-Agent") != "rtorrent/0.9.6/0.13.6" or h.get("Accept") != "*/*" or \
                h.get("Accept-Encoding") != "deflate, gzip":
            raise RuntimeError(f"Header rTorrent inattesi: {h}")
        pairs = [p.split("=", 1) for p in item["query"].split("&")]
        names = [n for n, _ in pairs]
        if [n for n in names if n in order] != order:
            raise RuntimeError(f"Ordine parametri rTorrent inatteso: {names}")
        values = dict(pairs)
        raw = values["peer_id"]
        peer = unquote_to_bytes(raw)
        if len(peer) != 20 or not peer.startswith(b"-lt0D60-") or b"\0" in peer:
            raise RuntimeError(f"peer_id rTorrent non valido: {raw}")
        if any(c in "abcdef" for i, c in enumerate(raw) if i and raw[i - 1] == "%") or \
                any(raw[i + 2] in "abcdef" for i in range(len(raw) - 2) if raw[i] == "%"):
            raise RuntimeError(f"Encoding peer_id non maiuscolo: {raw}")
        if values.get("port") != "40000":
            peers.add(peer)
            keys.add(values["key"])
    if len(peers) != 1 or len(keys) != 1:
        raise RuntimeError(f"peer_id/key rTorrent non persistenti: {len(peers)} peer, {len(keys)} key")
    print(f"PASS: {len(requests)} annunci rTorrent conformi al profilo "
          "(header, ordine parametri, peer_id binario, encoding, persistenza)")


if __name__ == "__main__":
    main()
