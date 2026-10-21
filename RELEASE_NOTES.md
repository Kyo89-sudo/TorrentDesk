## TorrentDesk 0.4.0 Preview

A native Windows x64 educational application for exploring tracker announces.

### New in 0.4.0
- **UDP trackers (BEP 15):** torrents with only `udp://` trackers now announce; bounded retries and clear error messages.
- **Tracker failover:** if the tracker in use fails, the next ones in the torrent are tried and the working one is kept.
- **HTTP redirects** followed (up to 2, never HTTPS→HTTP).
- **Torrent list:** sortable columns with remembered widths, new Tracker column, right-click menu (Start/Pause, Remove, Copy name/infohash), Del and Ctrl+A.
- **rTorrent 0.9.6 profile** supported by the engine (marked experimental): binary peer_id, upper-case URL encoding, persistent peer_id/key.
- **Client profiles trimmed to 28 recent ones:** qBittorrent 4.6–5.2, Deluge 2.x, Transmission 3.00, rTorrent 0.9.6. Default: qBittorrent 5.2.4.
- **Refreshed interface:** navy header in the Flow logo colours with the version number, flat command icons in the logo gradient, compact grouped buttons, Start all / Pause all when nothing is selected, tooltips.
- **Close-to-tray setting:** choose whether X hides to the tray or quits.
- Conformance tests against the local HTTP/UDP tracker.

### Downloads
Use **Setup-x64.exe** for installation or **portable-x64.zip** to extract and run. Verify packages using the attached SHA256SUMS file. Binaries are not digitally signed.

### Scope
This is a preview. Upload and speed are simulated; no P2P payload is transferred. HTTP/HTTPS and UDP announces are real. Pure v2 torrents are not supported. The profiles are not certified against current client/OS combinations.

Apache-2.0. See LICENSE and NOTICE for third-party attribution.
