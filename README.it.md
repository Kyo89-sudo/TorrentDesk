<p align="center"><img src="assets/flow-logo.png" alt="TorrentDesk logo" width="160"></p>

# TorrentDesk

Applicazione didattica **C++17 / Win32 per Windows x64**. Interfaccia italiana e inglese, nessun Java o Electron.

**Versione 0.4.0: prototipo funzionante, compatibilità parziale.** Studia parsing bencode, infohash SHA-1, richieste announce HTTP/HTTPS e UDP, scheduling e GUI nativa. Upload e velocità sono **simulati**: non trasferisce il contenuto dei file ad altri peer. Gli annunci al tracker sono richieste reali.

## Funzioni

- Importazione multipla di `.torrent` v1 e ibridi, anche tramite drag-and-drop.
- Nome, dimensione, velocità simulata, upload, ratio, seed/leecher e conto alla rovescia.
- Avvia, pausa, rimuovi; senza selezione i pulsanti diventano Avvia tutti / Pausa tutti. Suggerimenti su ogni comando.
- Tray e, se attiva in Impostazioni, la X nascondono la finestra nell'area di notifica.
- Velocità minima/massima globali in KiB/s, limite sessioni, ratio obiettivo, porta dichiarata.
- Annunci started / periodici / stopped, intervalli del tracker, retry con attesa crescente.
- Salvataggio atomico di configurazione, elenco torrent e upload storico; ripristino in pausa.
- Installer per utente, senza diritti amministratore. Icona originale e UI essenziale.

## Limiti espliciti

Inclusi 28 profili client, tutti supportati dal motore: qBittorrent 4.6–5.2, Deluge 2.x, Transmission 3.00 e rTorrent 0.9.6 (sperimentale). Il profilo predefinito è qBittorrent 5.2.4. I client vecchi o non supportati sono stati rimossi. **I profili non sono stati certificati per i client/OS attuali**: il nome di un file non è una prova di conformità. Gli esperimenti e i test forniti usano un tracker locale controllato.

Supporta tracker HTTP/HTTPS e UDP (BEP 15). Non supporta torrent v2 puri, download P2P, DHT, ascolto di connessioni peer, proxy configurabile, autenticazione HTTP o redirect. La porta è solo dichiarata nell'annuncio. Usa il primo tracker HTTP/HTTPS o UDP del torrent; non è ancora implementato il failover tra tracker. Gli stopped in chiusura sono best effort con un budget temporale. Il contatore “annunciato” indica il cumulativo nell'ultimo announce riuscito per sessione, non un trasferimento P2P.

## Compilazione

Richiede Visual Studio 2022/2026 o Build Tools con **Desktop development with C++** e Windows SDK. Le dipendenze del programma sono già nel repository; la build non scarica pacchetti.

Da Developer Command Prompt x64:

```bat
build.cmd
```

Oppure con CMake:

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

L'eseguibile deve avere la cartella `profiles` accanto. La build CMake la copia automaticamente.

## Prova didattica locale

Python 3 è necessario solo per gli strumenti di test:

```bat
python tools/local_tracker.py --port 18765
```

In un altro terminale:

```bat
python tools/make_example.py examples/local-demo.torrent
TorrentDesk.exe --data-dir work-demo examples/local-demo.torrent
```

Premi **Avvia**. Il tracker locale restituisce seed/leecher e intervallo; puoi osservare l'aumento dei contatori simulati e l'annuncio stopped premendo Pausa. Il torrent generato ha metadati sintetici e non contiene dati scaricabili. Il tracker ascolta esclusivamente su `127.0.0.1`.

Test di integrazione automatici (usano solo loopback):

```bat
python tools/run_integration.py TorrentDeskTests.exe .
```

## Dati e pacchetti

Dati predefiniti: `%LOCALAPPDATA%\TorrentDesk`. `--data-dir PERCORSO` sceglie una cartella separata. Il pacchetto portable contiene `portable.flag` e salva in `data` accanto all'eseguibile. L'installer non installa quel flag. La disinstallazione conserva i dati dell'utente.

```bat
ISCC installer.iss
```

Con Inno Setup 6 genera `dist/TorrentDesk-0.4.0-Setup-x64.exe`. La CI produce installer, portable e SHA256SUMS come artifact. L'eseguibile non è firmato digitalmente.

## Struttura e licenze

`src/torrent.hpp`: bencode e metadati; `src/engine.hpp`: protocollo e sessioni; `src/udp.hpp`: client tracker UDP (BEP 15); `src/main.cpp`: GUI; `src/tests.cpp`: regressioni. `tools/`: tracker e dimostrazioni. La CI Windows compila e verifica i test.

Apache-2.0, vedere [LICENSE](LICENSE) e [NOTICE](NOTICE). Profili client e convenzioni peer/key derivati da lavoro di terzi Apache-2.0 (vedere NOTICE); `nlohmann/json` è MIT. Per contribuire, vedere [CONTRIBUTING.md](CONTRIBUTING.md).

## Novità 0.4.0

Tracker UDP (BEP 15). Profilo rTorrent 0.9.6 supportato (sperimentale); profili ridotti a 28 client recenti, predefinito qBittorrent 5.2.4. Intestazione blu nei colori del logo con numero di versione, icone piatte, pulsanti raggruppati con Avvia tutti/Pausa tutti e suggerimenti. Impostazione per scegliere se la X riduce nella tray o chiude il programma. Test di conformità con il tracker locale HTTP e UDP.

## Novità 0.3.0

Logo Flow e icone coordinate, pulsante Tray, X che nasconde nell'area di notifica, menu tray con Apri/Avvia tutti/Pausa tutti/Aiuto/Esci. Solo Esci chiude il programma. Tab Aiuto con logo, informazioni e documentazione locale. Italiano e inglese selezionabili senza riavvio, scelta salvata in ui.json. Installer bilingue. Altre lingue: vedere LANGUAGES.md.
