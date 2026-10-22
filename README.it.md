<p align="center"><img src="assets/flow-logo.png" alt="TorrentDesk logo" width="140"></p>
<h1 align="center">TorrentDesk</h1>
<p align="center"><a href="https://github.com/Kyo89-sudo/TorrentDesk/actions/workflows/windows.yml"><img src="https://github.com/Kyo89-sudo/TorrentDesk/actions/workflows/windows.yml/badge.svg" alt="Windows build"></a> <a href="https://github.com/Kyo89-sudo/TorrentDesk/releases"><img src="https://img.shields.io/github/v/release/Kyo89-sudo/TorrentDesk?include_prereleases&label=release" alt="Release"></a> <img src="https://img.shields.io/badge/platform-Windows%20x64-0a1b42" alt="Windows x64"> <img src="https://img.shields.io/badge/license-Apache--2.0-blue" alt="License"></p>
<p align="center"><b>Simulatore nativo di annunci torrent per Windows — C++17 / Win32, tracker HTTP/HTTPS e UDP, italiano e inglese.</b><br>
<a href="README.md">English</a> · <a href="CHANGELOG.md">Novità</a> · <a href="https://github.com/Kyo89-sudo/TorrentDesk/releases">Download</a> · <a href="LANGUAGES.md">Traduzioni</a></p>

TorrentDesk è un'applicazione didattica che carica file `.torrent` e dialoga con i loro tracker come farebbe un client BitTorrent: annunci started, periodici e stopped, intervalli, seed/leecher. **Upload e velocità sono simulati** — nessun contenuto viene mai trasferito ai peer — mentre le richieste ai tracker sono reali. È un unico eseguibile nativo: niente Java, .NET, Electron o runtime da installare.

> **Anteprima (0.4.2).** Prototipo funzionante con compatibilità client parziale. Vedi [Limiti](#limiti).

## Download

Scarica l'ultima versione da [Release](https://github.com/Kyo89-sudo/TorrentDesk/releases):

| File | Uso |
|---|---|
| `TorrentDesk-x.y.z-Setup-x64.exe` | Installer per utente (senza diritti amministratore), italiano/inglese |
| `TorrentDesk-x.y.z-portable-x64.zip` | Estrai e avvia; i dati restano nella cartella `data` accanto all'exe |
| `SHA256SUMS.txt` | Hash per verificare i file scaricati |

Richiede Windows 10/11 x64. Gli eseguibili non sono ancora firmati, quindi SmartScreen può mostrare un avviso al primo avvio.

## Per iniziare

1. Aggiungi i `.torrent` con **Aggiungi…** o trascinandoli nella finestra.
2. In **Impostazioni** scegli range di upload, sessioni simultanee, ratio obiettivo e profilo client.
3. Premi **Avvia tutti** (oppure seleziona le righe e premi **Avvia**). Osserva stato, upload simulato, ratio, seed/leecher e prossimo annuncio.
4. **Pausa** invia l'annuncio stopped. La **X**, se configurata nelle Impostazioni, nasconde la finestra mentre le sessioni continuano.

Arrivi da JOAL? **Impostazioni → Importa configurazione…** legge il suo `config.json`; poi trascina i `.torrent` dalla sua cartella `torrents`.

## Funzioni

**Tracker**
- Tracker HTTP/HTTPS e **UDP (BEP 15)**; torrent v1 e ibridi.
- **Failover**: prima il tracker in uso, poi gli altri del torrent (massimo 4 per tentativo); quello funzionante viene mantenuto.
- Redirect HTTP seguiti (massimo 2, mai da HTTPS a HTTP); risposte gzip/deflate.
- Intervalli del tracker rispettati, attesa crescente sugli errori, stopped in chiusura best effort.

**Lista torrent**
- Colonne: nome, stato, dimensione, velocità, upload, ratio, seed/leech, prossimo annuncio, tracker.
- Clic sull'intestazione per ordinare; ordinamento e larghezza colonne vengono ricordati.
- Menu tasto destro: Avvia/Pausa, Rimuovi, Copia nome, Copia infohash. **Canc** rimuove (con conferma), **Ctrl+A** seleziona tutto.
- Senza selezione i pulsanti diventano **Avvia tutti / Pausa tutti**.
- Area dettagli ridimensionabile con Dettagli, Registro attività, Profili e contatori, Aiuto.

**Desktop**
- Interfaccia Win32 nativa, adatta all'alta risoluzione, intestazione blu nei colori del logo Flow, icone piatte, suggerimenti.
- Italiano e inglese, cambio immediato e ricordato.
- Icona nell'area di notifica con Apri, Avvia tutti, Pausa tutti, Aiuto, Esci. Impostazione per scegliere se la **X** riduce nella tray o chiude.
- Salvataggio atomico della sessione; i torrent vengono ripristinati in pausa.

## Profili client

29 profili, tutti supportati dal motore:

| Client | Versioni |
|---|---|
| qBittorrent | 4.6.0 – 5.2.4 (**predefinito: 5.2.4**) |
| Deluge | 2.0.3, 2.1.1 |
| Transmission | 3.00, 4.1.3 |
| rTorrent | 0.9.6 / libtorrent 0.13.6 — *sperimentale* |

Un profilo definisce peer_id, key, query e header HTTP. I profili **non sono certificati** per le combinazioni client/OS attuali; il numero di versione non è una prova di conformità. UDP non ha header, quindi lì il profilo si riflette solo in peer_id e key.

## Limiti

- L'upload è simulato: nessun dato viene scaricato o inviato, niente DHT né ascolto di peer (la porta annunciata non viene aperta).
- I torrent sono dichiarati completi (`left=0`) senza verificare i file.
- Non supporta torrent v2 puri, proxy configurabile e autenticazione HTTP. UDP non usa il proxy di sistema.
- I tracker privati hanno regole proprie su client ammessi e statistiche dichiarate: leggile prima di usare qualsiasi tracker.

## Dati e riga di comando

Dati utente: `%LOCALAPPDATA%\TorrentDesk` (portable: `data` accanto all'exe). Contiene `state.json` (impostazioni, torrent, contatori), `ui.json` (lingua, comportamento tray, colonne, altezza pannello) e le copie dei torrent importati. La disinstallazione li conserva.

| Opzione | Effetto |
|---|---|
| `--data-dir PERCORSO` | Usa una cartella dati separata |
| `--quit --data-dir PERCORSO` | Chiede all'istanza che usa quella cartella di chiudersi correttamente |
| `--ui-test --data-dir PERCORSO` | Smoke test dell'interfaccia; scrive `ui-smoke.json` |
| `file.torrent …` | Aggiunge torrent all'avvio |

## Compilare dai sorgenti

Visual Studio 2022/2026 o Build Tools con **Desktop development with C++** e Windows SDK. Tutte le dipendenze sono nel repository; la build non scarica nulla.

```bat
build.cmd
```

oppure con CMake:

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`profiles`, `languages`, `docs` e `assets\flow-logo.png` devono stare accanto a `TorrentDesk.exe` (CMake li copia). Le icone si rigenerano con `python src/make_icon.py` (Pillow), facoltativo.

## Test

`TorrentDeskTests.exe` verifica bencode, infohash, profili, generazione di peer_id/key, configurazione, persistenza e cataloghi. L'integrazione avvia un tracker locale su `127.0.0.1:18765` (HTTP e UDP) e controlla started/periodici/stopped, gzip, errori, redirect, failover, annunci UDP e conformità delle richieste rTorrent:

```bat
python tools/run_integration.py TorrentDeskTests.exe .
```

Per provare l'app solo contro il tracker locale:

```bat
python tools/local_tracker.py
python tools/make_example.py examples/local-demo.torrent
TorrentDesk.exe --data-dir work-demo examples/local-demo.torrent
```

GitHub Actions compila, esegue tutti i test e produce installer, archivi portable/sorgenti e `SHA256SUMS.txt` a ogni push.

## Pacchetti e firma

```bat
ISCC installer.iss
python tools/package.py
```

Con un certificato di firma del codice (token USB o HSM cloud dell'autorità), `sign.cmd IMPRONTA [URL_TIMESTAMP]` firma l'exe, ricrea e firma l'installer, poi genera portable e hash. L'impronta si trova con `certutil -user -store My`.

## Struttura del progetto

| Percorso | Contenuto |
|---|---|
| `src/main.cpp` | Interfaccia desktop |
| `src/engine.hpp` | Scheduler delle sessioni, annunci HTTP, failover |
| `src/udp.hpp` | Client tracker UDP (BEP 15) |
| `src/torrent.hpp` | Bencode, metadati, infohash |
| `src/i18n.hpp`, `languages/` | Cataloghi di traduzione |
| `src/tests.cpp`, `tools/` | Test, tracker locale, pacchetti |
| `profiles/` | Profili client |
| `docs/` | Guida locale e licenze |

Per contribuire: [CONTRIBUTING.md](CONTRIBUTING.md). Traduzioni: [LANGUAGES.md](LANGUAGES.md).

## Licenza

[Apache-2.0](LICENSE). Profili client e convenzioni peer/key derivano da lavoro di terzi Apache-2.0 — attribuzione in [NOTICE](NOTICE). [nlohmann/json](https://github.com/nlohmann/json) è MIT.
