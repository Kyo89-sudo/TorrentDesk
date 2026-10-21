# Changelog

All notable changes to TorrentDesk. Versions follow [Semantic Versioning](https://semver.org/); 0.x releases are previews.

## [0.4.1] — 2026-10-21

### Added
- Tracker failover: the tracker in use first, then the others in torrent order (up to 4 per attempt); the working one is kept and logged.
- HTTP redirects followed (up to 2); relative locations stay on the same host, HTTPS→HTTP is refused.
- Sortable columns with an arrow on the header; sort order and column widths saved in `ui.json`.
- Tracker column showing the host in use.
- Right-click menu on the list: Start/Pause (all), Remove, Copy name, Copy infohash. Del removes after confirmation, Ctrl+A selects all.
- `sign.cmd` for local Authenticode signing of the executable and installer.
- Transmission 4.1.3 profile, derived from the Transmission source (peer_id `-TR4130-` with checksum, 8-hex key, query order, unreserved characters).
- Resizable details area: drag the border between the list and the tabs; the height is remembered.
- Tests for redirects (with query, loop rejected) and failover from a failing tracker.

### Changed
- Remove asks for confirmation (button, menu and Del key).
- Tracker warnings and rejections now show the tracker's own message; rows with only a warning are amber, errors stay red.

## [0.4.0] — 2026-10-21

### Added
- UDP tracker support (BEP 15): connect/announce over Winsock, bounded retries, IPv4/IPv6 hosts.
- rTorrent 0.9.6 profile supported by the engine, marked experimental (binary peer_id, upper-case encoding, persistent peer_id/key).
- Setting to choose whether X hides to the tray or quits.
- Version number in the header, window title and Help tab.
- Tooltips on every command; Start/Pause read "Start all"/"Pause all" when nothing is selected.
- CI builds installer, portable and source archives plus `SHA256SUMS.txt`.
- Conformance tests against the local tracker: rTorrent requests and UDP announces.

### Changed
- Interface refresh: navy header in the Flow logo colours, flat command icons in the logo gradient, compact buttons grouped Add | Start · Pause · Remove | Settings · Tray.
- Client profiles trimmed from 95 to 28: qBittorrent 4.6–5.2, Deluge 2.x, Transmission 3.00, rTorrent 0.9.6. Default stays qBittorrent 5.2.4.
- Remove is disabled without a selection.
- "Import JOAL config" renamed "Import configuration…" (still reads JOAL-style `config.json`).
- Third-party references removed from UI and docs; attribution kept in `NOTICE` and the licences page.

### Removed
- uTorrent, BitTorrent, Vuze, Leap, Transmission 2.x and old qBittorrent/Deluge profiles.

## [0.3.0] — 2026-10-21

### Added
- Flow logo and coordinated icons, Tray button, close-to-tray, tray menu (Open, Start all, Pause all, Help, Exit).
- Help tab with logo, information and local documentation.
- English and Italian UI switchable without restart, saved in `ui.json`.
- Bilingual per-user installer and portable package.

[0.4.1]: https://github.com/Kyo89-sudo/TorrentDesk/compare/v0.4.0...main
[0.4.0]: https://github.com/Kyo89-sudo/TorrentDesk/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/Kyo89-sudo/TorrentDesk/releases/tag/v0.3.0
