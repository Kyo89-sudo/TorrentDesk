<p align="center"><img src="assets/flow-logo.png" alt="TorrentDesk logo" width="140"></p>
<h1 align="center">TorrentDesk</h1>
<p align="center"><a href="https://github.com/Kyo89-sudo/TorrentDesk/actions/workflows/windows.yml"><img src="https://github.com/Kyo89-sudo/TorrentDesk/actions/workflows/windows.yml/badge.svg" alt="Windows build"></a> <a href="https://github.com/Kyo89-sudo/TorrentDesk/releases"><img src="https://img.shields.io/github/v/release/Kyo89-sudo/TorrentDesk?include_prereleases&label=release" alt="Release"></a> <img src="https://img.shields.io/badge/platform-Windows%20x64-0a1b42" alt="Windows x64"> <img src="https://img.shields.io/badge/license-Apache--2.0-blue" alt="License"></p>
<p align="center"><b>Native Windows torrent announce simulator — C++17 / Win32, HTTP/HTTPS and UDP trackers, English and Italian.</b><br>
<a href="README.it.md">Italiano</a> · <a href="CHANGELOG.md">Changelog</a> · <a href="https://github.com/Kyo89-sudo/TorrentDesk/releases">Download</a> · <a href="LANGUAGES.md">Translations</a></p>

TorrentDesk is an educational desktop app that loads `.torrent` files and talks to their trackers the way a BitTorrent client does: started, periodic and stopped announces, intervals, seeders/leechers. **Upload and speed are simulated** — no file content is ever transferred to peers — while the tracker requests are real. It is a single native executable: no Java, .NET, Electron or runtime to install.

> **Preview (0.4.1).** Working prototype with partial client compatibility. See [Limitations](#limitations).

## Download

Get the latest version from [Releases](https://github.com/Kyo89-sudo/TorrentDesk/releases):

| File | Use |
|---|---|
| `TorrentDesk-x.y.z-Setup-x64.exe` | Per-user installer (no admin rights), Italian/English |
| `TorrentDesk-x.y.z-portable-x64.zip` | Extract and run; data stays in a `data` folder next to the exe |
| `SHA256SUMS.txt` | Checksums to verify the downloads |

Requires Windows 10/11 x64. Binaries are not code-signed yet, so SmartScreen may warn on first run.

## Quick start

1. Add `.torrent` files with **Add…** or drag them onto the window.
2. Pick the upload range, simultaneous sessions, ratio target and client profile in **Settings**.
3. Press **Start all** (or select rows and press **Start**). Watch status, simulated upload, ratio, seeders/leechers and the next announce.
4. **Pause** sends the stopped announce. **Tray** hides the window while sessions continue.

Coming from JOAL? **Settings → Import configuration…** reads its `config.json`; then drag in the `.torrent` files from its `torrents` folder.

## Features

**Trackers**
- HTTP/HTTPS and **UDP (BEP 15)** trackers; v1 and hybrid torrents.
- **Failover**: the tracker in use first, then the others in the torrent (up to 4 per attempt); the working one is kept.
- HTTP redirects followed (up to 2, never HTTPS → HTTP); gzip/deflate responses.
- Tracker intervals honoured, growing back-off on errors, best-effort stopped announces on exit.

**Torrent list**
- Columns: name, status, size, speed, upload, ratio, seed/leech, next announce, tracker.
- Click a header to sort; sort order and column widths are remembered.
- Right-click menu: Start/Pause, Remove, Copy name, Copy infohash. **Del** removes (with confirmation), **Ctrl+A** selects all.
- With nothing selected the buttons read **Start all / Pause all**.
- Resizable details area with Details, Activity log, Profiles and counters, Help.

**Desktop**
- Native Win32 UI, high-DPI aware, navy header in the Flow logo colours, flat icons, tooltips.
- English and Italian, switchable instantly; the choice is remembered.
- Notification-area icon with Open, Start all, Pause all, Help, Exit. Setting to choose whether **X** hides to the tray or quits.
- Atomic session saving; torrents are restored paused.

## Client profiles

29 profiles, all supported by the engine:

| Client | Versions |
|---|---|
| qBittorrent | 4.6.0 – 5.2.4 (**default: 5.2.4**) |
| Deluge | 2.0.3, 2.1.1 |
| Transmission | 3.00, 4.1.3 |
| rTorrent | 0.9.6 / libtorrent 0.13.6 — *experimental* |

A profile defines peer_id, key, query and HTTP headers. Profiles are **not certified** against current client/OS combinations; a version string is not proof of conformity. UDP has no headers, so there only peer_id and key reflect the profile.

## Limitations

- Upload is simulated; no payload is downloaded or uploaded, no DHT, no peer listener (the announced port is not opened).
- Torrents are announced complete (`left=0`) without checking files.
- Pure v2 torrents, configurable proxy and HTTP authentication are not supported. UDP does not use the system proxy.
- Private trackers have their own rules on allowed clients and reported statistics: read them before using any tracker.

## Data and command line

User data: `%LOCALAPPDATA%\TorrentDesk` (portable: `data` next to the exe). It holds `state.json` (settings, torrents, counters), `ui.json` (language, tray behaviour, columns, panel height) and copies of the imported torrents. Uninstalling keeps it.

| Option | Effect |
|---|---|
| `--data-dir PATH` | Use a separate data folder |
| `--quit --data-dir PATH` | Ask a running instance using that folder to exit cleanly |
| `--ui-test --data-dir PATH` | Desktop smoke test; writes `ui-smoke.json` |
| `file.torrent …` | Add torrents at startup |

## Build from source

Visual Studio 2022/2026 or Build Tools with **Desktop development with C++** and the Windows SDK. All dependencies are in the repository; the build downloads nothing.

```bat
build.cmd
```

or with CMake:

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`profiles`, `languages`, `docs` and `assets\flow-logo.png` must sit next to `TorrentDesk.exe` (CMake copies them). Icons can be regenerated with `python src/make_icon.py` (Pillow), optional.

## Tests

`TorrentDeskTests.exe` covers bencode, infohashes, profiles, peer_id/key generation, configuration, persistence and catalogs. The integration run starts a local tracker on `127.0.0.1:18765` (HTTP and UDP) and checks started/periodic/stopped, gzip, errors, redirects, failover, UDP announces and rTorrent request conformance:

```bat
python tools/run_integration.py TorrentDeskTests.exe .
```

Try the app against the local tracker only:

```bat
python tools/local_tracker.py
python tools/make_example.py examples/local-demo.torrent
TorrentDesk.exe --data-dir work-demo examples/local-demo.torrent
```

GitHub Actions builds, runs all tests and produces the installer, portable/source archives and `SHA256SUMS.txt` on every push.

## Packaging and signing

```bat
ISCC installer.iss
python tools/package.py
```

With a code-signing certificate (USB token or CA cloud HSM), `sign.cmd THUMBPRINT [TIMESTAMP_URL]` signs the exe, rebuilds and signs the installer, then creates the portable archive and checksums. Find the thumbprint with `certutil -user -store My`.

## Project layout

| Path | Content |
|---|---|
| `src/main.cpp` | Desktop UI |
| `src/engine.hpp` | Session scheduler, HTTP announces, failover |
| `src/udp.hpp` | UDP tracker client (BEP 15) |
| `src/torrent.hpp` | Bencode, metadata, infohashes |
| `src/i18n.hpp`, `languages/` | Translation catalogs |
| `src/tests.cpp`, `tools/` | Tests, local tracker, packaging |
| `profiles/` | Client profiles |
| `docs/` | Local help and licences |

Contributions: see [CONTRIBUTING.md](CONTRIBUTING.md). Translations: see [LANGUAGES.md](LANGUAGES.md).

## License

[Apache-2.0](LICENSE). Client profiles and peer/key conventions derive from third-party Apache-2.0 work — attribution in [NOTICE](NOTICE). [nlohmann/json](https://github.com/nlohmann/json) is MIT.
