<p align="center"><img src="assets/flow-logo.png" alt="TorrentDesk logo" width="160"></p>

# TorrentDesk

[Italiano](README.it.md) · [Translations](LANGUAGES.md)

**Educational Windows x64 application, written in C++17 with a native Win32 UI.** No Java or Electron runtime.

Version **0.4.0 is a working prototype with partial protocol/client compatibility**. It explores bencode parsing, SHA-1 infohashes, HTTP/HTTPS and UDP tracker announces, scheduling and desktop UI. Upload and speed are simulated; it does not transfer payload to peers. Tracker requests are real.

## Desktop features

- Flow logo, navy header in the logo palette, flat command icons and the version shown in the header.
- English and Italian UI, switchable immediately with a remembered preference.
- Multiple .torrent imports and drag-and-drop; v1 and hybrid metadata.
- Size, status, simulated speed/upload/ratio, seeders/leechers and announce countdown.
- Start, pause, remove; with no selection the buttons read Start all / Pause all. Tooltips on every command.
- Global upload range, simultaneous-session limit, ratio target and announced port.
- **Tray** (and X, unless turned off in Settings) hides the window while sessions continue. Click the tray icon to reopen it; its menu offers Open, Start all, Pause all, Help and **Exit**.
- Help tab with logo, version, credits, local documentation and licenses.
- Atomic session persistence; torrents restart paused. A per-user bilingual installer and a portable package.

## Explicit limitations

28 client profiles are included, all supported by the native engine: qBittorrent 4.6–5.2, Deluge 2.x, Transmission 3.00 and rTorrent 0.9.6 (experimental). The default profile is qBittorrent 5.2.4. Older and unsupported clients have been removed. These profiles are **not certified against current client/OS combinations**; a filename/version string is not proof of conformity.

HTTP/HTTPS and UDP (BEP 15) trackers; no pure v2 torrents, payload downloads, DHT, peer listener, configurable proxy, HTTP authentication or redirects. The advertised port is not a listening port. Uses the first HTTP/HTTPS or UDP tracker; tracker failover is not implemented. Torrents are announced complete (`left=0`) without checking files. Shutdown stopped announces are best effort with a time budget.

## Build

Visual Studio 2022/2026 or Build Tools, **Desktop development with C++**, Windows SDK. Runtime dependencies are included in the source tree; the build does not download packages.

```bat
build.cmd
```

Or use CMake:

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CMake copies profiles, language catalogs and help files beside the executable. For manual builds keep those folders beside TorrentDesk.exe. To regenerate icon files from the Flow logo and the flat command glyphs, run `python src/make_icon.py` with Pillow installed; this is optional and is not required to build the application.

## Local learning example

Python 3 is required only for these test tools. In one terminal:

```bat
python tools/local_tracker.py
```

In another:

```bat
python tools/make_example.py examples/local-demo.torrent
TorrentDesk.exe --data-dir work-demo examples/local-demo.torrent
```

Click **Start** and observe simulated counters and real announce responses. The generated torrent is synthetic metadata without downloadable payload. The example tracker binds only to `127.0.0.1`.

```bat
python tools/run_integration.py TorrentDeskTests.exe .
```

This tests only a controlled local tracker (HTTP and UDP on the same port), including started/periodic/stopped, gzip, pause, errors, redirects and rTorrent profile conformance. The desktop smoke test requires a real Windows desktop with a notification area:

```bat
TorrentDesk.exe --ui-test --data-dir work-ui-test
```

It switches language, opens Help, hides/restores through tray callbacks, selects Exit and writes `ui-smoke.json` with the results. `--quit --data-dir PATH` requests graceful exit of an instance using that data directory.

## Data and packaging

Default user data: `%LOCALAPPDATA%\TorrentDesk`; `--data-dir PATH` selects a separate folder. Portable archives include `portable.flag` and use a sibling `data` directory. Installed builds do not use that flag. Uninstall preserves user data.

```bat
ISCC installer.iss
python tools/package.py
```

Inno Setup 6 creates `dist/TorrentDesk-0.4.0-Setup-x64.exe`. CI builds the installer, portable archive and SHA256SUMS as an artifact. The packaging script creates clean portable/source archives without sessions, imported torrents or logs. Binaries are not digitally signed. Language catalogs and HTML help are shipped with both distributions.

### Signing (optional)

With a code-signing certificate on a USB token or a CA cloud HSM, run `build.cmd` and then:

```bat
sign.cmd CERTIFICATE_SHA1_THUMBPRINT [TIMESTAMP_URL]
```

It signs `TorrentDesk.exe`, builds the installer with the signed executable, signs the setup, then creates the portable archive and `dist\SHA256SUMS.txt`. Requires Windows SDK (signtool), Inno Setup 6 and Python 3. Find the thumbprint with `certutil -user -store My`. The default timestamp server is Certum's.

## Source and licensing

`src/torrent.hpp`: parsing and infohashes; `src/engine.hpp`: protocol/session engine; `src/udp.hpp`: UDP tracker client (BEP 15); `src/main.cpp`: desktop UI; `src/i18n.hpp`: catalog loading; `src/tests.cpp`: regressions. Windows CI builds and tests the project. See [CONTRIBUTING.md](CONTRIBUTING.md).

Apache-2.0: [LICENSE](LICENSE), [NOTICE](NOTICE). Client profiles and peer/key conventions are derived from third-party Apache-2.0 work (see NOTICE); nlohmann/json is MIT. The original Flow logo was created with imagegen and selected for this project; the command icons are generated by `src/make_icon.py`. Nothing is automatically published to GitHub.
