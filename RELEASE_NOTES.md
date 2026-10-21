## TorrentDesk 0.4.1 Preview

Update to 0.4.0 with tracker reliability and torrent-list improvements.

### New in 0.4.1
- **Tracker failover:** if the tracker in use fails, the next ones in the torrent are tried (up to 4 per attempt) and the working one is kept.
- **HTTP redirects** followed (up to 2, never HTTPS→HTTP).
- **Sortable columns** with remembered sort order and widths.
- **Tracker column** with the host in use.
- **Resizable details area:** drag the border between the list and the tabs; the height is remembered.
- **Right-click menu:** Start/Pause, Remove, Copy name, Copy infohash. **Del** removes after confirmation, **Ctrl+A** selects all.
- `sign.cmd` for local code signing (for maintainers).

See CHANGELOG.md for the full list, including 0.4.0 (UDP trackers, rTorrent profile, new interface, close-to-tray setting).

### Downloads
Use **Setup-x64.exe** for installation or **portable-x64.zip** to extract and run. Verify packages using the attached SHA256SUMS file. Binaries are not digitally signed.

### Scope
This is a preview. Upload and speed are simulated; no P2P payload is transferred. HTTP/HTTPS and UDP announces are real. Pure v2 torrents are not supported. The profiles are not certified against current client/OS combinations.

Apache-2.0. See LICENSE and NOTICE for third-party attribution.
