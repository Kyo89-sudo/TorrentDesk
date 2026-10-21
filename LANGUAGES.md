# Translation catalogs

Included: English (`en.json`) and Italian (`it.json`). The app chooses Italian on an Italian Windows UI and English otherwise. The header selector changes language immediately and saves the choice in the user data folder's `ui.json`.

To add a language, copy `languages/en.json` to a code such as `fr.json` or `pt-BR.json`. Change `language` and `name` (the native display name), then translate values under `strings`; keep every key unchanged. Keys currently use the exact original Italian source phrases. Preserve leading/trailing spaces and escaped line breaks, which are used when composing labels. Do not translate user-provided torrent names or paths.

The language selector discovers catalogs on startup. Missing translations fall back to English. Malformed catalogs are rejected. The embedded English resource provides a fallback for standalone startup. Test English and Italian key parity with TorrentDeskTests.exe. For a new catalog, validate every value as a string and check long labels, dialogs, logs and tray menus.

Add localized help as `docs/help.CODE.html` and `docs/licenses.CODE.html`, then extend the help-language resolver if needed. Currently local Help uses Italian for `it` and English for other languages. JSON catalogs are included automatically by the installer and package script.
