# TorrentDesk 0.3.0 validation

- MSVC script build and CMake Release build succeeded. CTest core suite passed.
- 165 English/Italian catalog entries: parity, language switching, multiline preservation, dynamic message prefixes and language path validation passed.
- Existing bencode, SHA-1 infohash, single/multifile, Unicode paths, configuration, duplicate and persistence regressions passed. 95 profiles parsed, 81 supported by the engine.
- Controlled loopback tracker integration passed: gzip responses, HTTP errors/redirect policy, started/periodic/stopped announces, simulated upload and pause.
- Real Windows desktop smoke test passed: English selection, persisted preference, Help page, tray creation, close-to-tray, restore via tray callback and Exit. Tray tests require an interactive desktop and cannot pass inside a restricted AppContainer without a notification area.
- Installer tested with English UI. English/Italian catalogs, HTML help and Flow logo installed correctly; uninstall completed and removed the test registration.
- Flow artwork has transparent exterior corners and is encoded as a multi-size ICO. Command icons use matching navy/blue/cyan artwork. No public/private external trackers were tested.

This remains a partial educational prototype. Current client/OS profile conformity, UDP, pure v2 and tracker failover are not validated or implemented; see README.md.
