## TorrentDesk 0.3.0 Preview

A native Windows x64 educational application for exploring tracker announces.

### Included
- Flow branding and coordinated command icons.
- Native C++17 / Win32 UI; no Java or Electron runtime.
- English and Italian UI with immediate switching and remembered preferences.
- Multiple torrent import, drag-and-drop, simulated upload/speed/ratio and tracker counters.
- Close-to-tray, restore, Start/Pause all and graceful Exit.
- Help tab with logo, version, credits and local bilingual documentation.
- Per-user installer, portable archive and clean source package.

### Downloads
Use **Setup-x64.exe** for installation or **portable-x64.zip** to extract and run. The source archive is provided for study and modification. Verify packages using the attached SHA256 file. Binaries are not digitally signed.

### Scope
This is a preview. Upload and speed are simulated; no P2P payload is transferred. HTTP/HTTPS announces are real. UDP and pure v2 torrents are not supported. The inherited profiles are not certified against current client/OS combinations.

### Validation
MSVC and CMake builds, core/catalog tests, controlled loopback tracker integration, real-desktop tray/language/Help smoke tests, installation and uninstallation were tested locally. GitHub Actions results are reported separately.

Apache-2.0. See LICENSE and NOTICE for third-party attribution.
