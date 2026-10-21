"""Avvia un tracker locale isolato, esegue i test nativi e lo arresta."""
import subprocess
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
        finally:
            server.terminate()
            server.wait(timeout=5)


if __name__ == "__main__":
    main()
