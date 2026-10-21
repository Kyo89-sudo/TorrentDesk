#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include "engine.hpp"
#include "i18n.hpp"
#include "version.hpp"
I18n locale;
std::wstring tr(const std::wstring &s) { return locale.get(s); }
int localizedMessage(HWND w, LPCWSTR body, LPCWSTR title, UINT type) {
    auto message = locale.message(body);
    auto caption = tr(title);
    return MessageBoxW(w, message.c_str(), caption.c_str(), type);
}
#undef MessageBox
#define MessageBox localizedMessage
#include <deque>
HINSTANCE appInstance;
HWND mainWindow, table, tabs, panel, statusbar, helpPane, languageCombo;
std::vector<std::pair<std::string, std::wstring>> languageChoices;
std::vector<HIMAGELIST> commandImages;
HIMAGELIST tabImages = nullptr;
NOTIFYICONDATA trayData = {sizeof(trayData)};
bool trayPresent = false;
UINT taskbarCreated = 0, restoreMessage = 0;
constexpr UINT trayCallback = WM_APP + 17;
void refresh(bool sample);
void applyLanguage();
void restoreWindow();
void hideToTray();
void beginExit();
HFONT uiFont, titleFont, smallFont;
HBRUSH backgroundBrush;
HICON appIcon, appLogoIcon;
fs::path base, dataDir;
std::unique_ptr<DeskEngine> engine;
ViewState current;
std::vector<fs::path> profileFiles;
std::vector<std::string> visibleIds;
UINT dpi = 96;
bool refreshLock = false, closing = false;
bool uiSmoke = false;
int uiStage = 0, uiTestExit = 0;
json uiResults = json::object();
std::atomic<bool> closed = false;
bool closeToTray = true; // X hides to the tray (setting, saved in ui.json)
constexpr int columnCount = 9;
int sortColumn = -1;       // -1 = order of addition
bool sortAscending = true;
bool userWidths = false;   // user resized columns: keep their widths, no auto-fit
std::vector<int> savedWidths; // in 96-dpi units, from ui.json
int panelHeight = 185;        // bottom area (tabs + details), 96-dpi units; draggable
bool draggingSplitter = false;
void saveUiPreferences() {
    auto temp = dataDir / L"ui.tmp";
    json ui = {{"language", locale.code()},
               {"closeToTray", closeToTray},
               {"sortColumn", sortColumn},
               {"sortAscending", sortAscending},
               {"panelHeight", panelHeight}};
    if (userWidths && table) {
        json widths = json::array();
        for (int i = 0; i < columnCount; ++i)
            widths.push_back(MulDiv(ListView_GetColumnWidth(table, i), 96, (int)dpi));
        ui["columnWidths"] = widths;
    }
    {
        std::ofstream out(temp);
        out << ui.dump(2);
        if (!out.flush())
            throw std::runtime_error("Cannot save UI preferences.");
    }
    if (!MoveFileEx(temp.c_str(), (dataDir / L"ui.json").c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot save UI preferences.");
}
std::thread closer;
const COLORREF ink = RGB(26, 48, 57), muted = RGB(91, 111, 120), bg = RGB(249, 250, 251);
int px(int v) { return MulDiv(v, (int)dpi, 96); }
void errorBox(HWND w, const std::exception &e) {
    MessageBox(w, wide(e.what()).c_str(), L"TorrentDesk", MB_ICONERROR);
}
std::wstring readText(HWND h) {
    int n = GetWindowTextLength(h);
    std::wstring v(n + 1, 0);
    GetWindowText(h, v.data(), n + 1);
    v.resize(n);
    return v;
}
void replaceText(HWND h, const std::wstring &s) {
    if (readText(h) != s)
        SetWindowText(h, s.c_str());
}
void setFonts() {
    if (uiFont)
        DeleteObject(uiFont);
    if (titleFont)
        DeleteObject(titleFont);
    if (smallFont)
        DeleteObject(smallFont);
    uiFont = CreateFont(-px(14), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                        CLEARTYPE_QUALITY, 0, L"Segoe UI");
    titleFont = CreateFont(-px(19), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                           CLEARTYPE_QUALITY, 0, L"Segoe UI");
    smallFont = CreateFont(-px(12), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                           CLEARTYPE_QUALITY, 0, L"Segoe UI");
    if (mainWindow)
        EnumChildWindows(
            mainWindow,
            [](HWND h, LPARAM) -> BOOL {
                SendMessage(h, WM_SETFONT, (WPARAM)uiFont, TRUE);
                return TRUE;
            },
            0);
}
void text(HDC dc, const std::wstring &s, RECT r, HFONT f, COLORREF c,
          UINT flags = DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS) {
    auto old = SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    auto translated = tr(s);
    DrawText(dc, translated.c_str(), (int)translated.size(), &r, flags);
    SelectObject(dc, old);
}
std::vector<std::string> selected() {
    std::vector<std::string> ids;
    for (int i = -1; (i = ListView_GetNextItem(table, i, LVNI_SELECTED)) != -1;)
        if ((size_t)i < visibleIds.size())
            ids.push_back(visibleIds[i]);
    return ids;
}
// Untranslated label of command button i (ids 100..105). Start/Pause read
// "… all" when nothing is selected, because they then act on every torrent.
const wchar_t *commandLabel(int i, bool all) {
    static const wchar_t *labels[] = {L"Aggiungi…", L"Avvia",        L"Pausa",
                                      L"Rimuovi",   L"Impostazioni", L"Tray"};
    if (all && i == 1)
        return L"Avvia tutti";
    if (all && i == 2)
        return L"Pausa tutti";
    return labels[i];
}
const wchar_t *commandTip(int i) {
    static const wchar_t *tips[] = {
        L"Aggiungi file .torrent (puoi anche trascinarli nella finestra)",
        L"Avvia i torrent selezionati, o tutti se nessuno è selezionato",
        L"Metti in pausa i torrent selezionati, o tutti se nessuno è selezionato",
        L"Rimuovi i torrent selezionati",
        L"Range di upload, sessioni simultanee, ratio, porta e profilo client",
        L"Nascondi nell'area di notifica; i torrent continuano"};
    return tips[i];
}
HWND tooltip = nullptr;
std::wstring tipBuffer;
void createTooltips(HWND w) {
    tooltip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_ALWAYSTIP,
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, w,
                             nullptr, appInstance, nullptr);
    if (!tooltip)
        return;
    for (int i = 0; i < 6; ++i) {
        TOOLINFO ti = {sizeof(ti)};
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = w;
        ti.uId = (UINT_PTR)GetDlgItem(w, 100 + i);
        ti.lpszText = LPSTR_TEXTCALLBACK;
        SendMessage(tooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
    }
    SendMessage(tooltip, TTM_SETMAXTIPWIDTH, 0, 420);
}
void updateCommands() {
    if (!mainWindow || closing)
        return;
    bool none = ListView_GetSelectedCount(table) == 0;
    bool any = !current.rows.empty();
    for (int i = 0; i < 6; ++i)
        replaceText(GetDlgItem(mainWindow, 100 + i), tr(commandLabel(i, none)));
    EnableWindow(GetDlgItem(mainWindow, 101), any);
    EnableWindow(GetDlgItem(mainWindow, 102), any);
    EnableWindow(GetDlgItem(mainWindow, 103), !none);
}
void layout() {
    RECT r;
    GetClientRect(mainWindow, &r);
    int width = r.right, height = r.bottom;
    // The bottom area is resizable with the splitter between list and tabs; Help needs 260.
    int bottom = std::max(panelHeight, TabCtrl_GetCurSel(tabs) == 3 ? 260 : 150);
    int top = px(108);
    // The list always keeps at least 40% of the space (and 200 px) below the buttons.
    int tabY = std::max(top + std::max(px(200), (height - top) * 2 / 5), height - px(bottom));
    int area = std::max(px(77), height - tabY - px(73));
    MoveWindow(table, px(16), top, width - px(32), std::max(px(140), tabY - top - px(10)), TRUE);
    MoveWindow(tabs, px(16), tabY, width - px(32), px(28), TRUE);
    MoveWindow(panel, px(18), tabY + px(34), width - px(36), area, TRUE);
    MoveWindow(helpPane, px(18), tabY + px(34), width - px(36), area, TRUE);
    MoveWindow(languageCombo, width - px(164), px(13), px(148), px(250), TRUE);
    MoveWindow(statusbar, 0, height - px(27), width, px(27), TRUE);
    int parts[] = {px(235), px(485), px(740), -1};
    SendMessage(statusbar, SB_SETPARTS, 4, (LPARAM)parts);
    // Command buttons sized to their (translated) labels, in three groups:
    // [Add]   [Start][Pause][Remove]          ...          [Settings][Tray]
    int widths[6];
    {
        auto dc = GetDC(mainWindow);
        auto old = SelectObject(dc, uiFont);
        for (int i = 0; i < 6; ++i) {
            int w = 0;
            for (int v = 0; v < 2; ++v) {
                auto s = tr(commandLabel(i, v == 1));
                SIZE sz = {};
                GetTextExtentPoint32(dc, s.c_str(), (int)s.size(), &sz);
                w = std::max(w, (int)sz.cx);
            }
            widths[i] = std::max(px(88), w + px(16 + 8 + 22));
        }
        SelectObject(dc, old);
        ReleaseDC(mainWindow, dc);
    }
    int gap = px(6), group = px(18), y = px(66), h = px(30);
    int x = px(16);
    for (int i = 0; i < 4; ++i) {
        MoveWindow(GetDlgItem(mainWindow, 100 + i), x, y, widths[i], h, TRUE);
        x += widths[i] + (i == 0 ? group : gap);
    }
    int right = width - px(16);
    int left = std::max(x + group, right - widths[4] - gap - widths[5]);
    MoveWindow(GetDlgItem(mainWindow, 104), left, y, widths[4], h, TRUE);
    MoveWindow(GetDlgItem(mainWindow, 105), left + widths[4] + gap, y, widths[5], h, TRUE);
    if (!userWidths) {
        int fixed = px(125 + 90 + 95 + 95 + 65 + 80 + 75 + 120);
        ListView_SetColumnWidth(table, 0, std::max(px(170), width - px(50) - fixed));
    }
    InvalidateRect(mainWindow, nullptr, FALSE);
}
// Header band in the Flow logo palette: navy bar, blue->cyan accent line.
const COLORREF navy = RGB(10, 27, 66), navyText = RGB(150, 190, 235);
void paintHeader(HDC dc) {
    RECT r;
    GetClientRect(mainWindow, &r);
    FillRect(dc, &r, backgroundBrush);
    RECT band = {0, 0, r.right, px(52)};
    auto navyBrush = CreateSolidBrush(navy);
    FillRect(dc, &band, navyBrush);
    DeleteObject(navyBrush);
    for (int x = 0; x < r.right; x += 4) {
        double t = r.right ? (double)x / r.right : 0;
        auto line = CreateSolidBrush(RGB(0, (int)(96 + 120 * t), (int)(240 - 10 * t)));
        RECT seg = {x, px(52), std::min(x + 4, (int)r.right), px(52) + std::max(2, px(2))};
        FillRect(dc, &seg, line);
        DeleteObject(line);
    }
    DrawIconEx(dc, px(16), px(14), appIcon, px(24), px(24), 0, nullptr, DI_NORMAL);
    text(dc, L"TorrentDesk", {px(48), px(8), px(320), px(44)}, titleFont, RGB(255, 255, 255));
    {
        auto old = SelectObject(dc, titleFont);
        SIZE sz = {};
        GetTextExtentPoint32(dc, L"TorrentDesk", 11, &sz);
        SelectObject(dc, old);
        int vx = px(48) + sz.cx + px(8);
        text(dc, std::wstring(L"v") + TORRENTDESK_VERSION, {vx, px(13), vx + px(120), px(42)},
             smallFont, navyText);
    }
    text(dc, L"Simulazione upload", {r.right - px(430), px(10), r.right - px(180), px(42)},
         smallFont, navyText, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
}
std::wstring describe(const TorrentRow &r) {
    std::wstring out = tr(L"Nome: ") + wide(r.meta.name) + tr(L"\r\nDimensione: ") +
                       sizeText(r.meta.size) + L"   ·   File: " + std::to_wstring(r.meta.files) +
                       L"   ·   Infohash: " + wide(hex(r.meta.hash)) +
                       tr(L"\r\nProfilo sessione: ") +
                       wide(r.profileName.empty() ? engine->getConfig().client : r.profileName) +
                       tr(L"   ·   Download payload: 0 B (nessun trasferimento P2P)\r\nUpload "
                          L"sessione simulato: ") +
                       sizeText(r.uploaded) + tr(L"   ·   Ultimo annuncio riuscito: ") +
                       sizeText(r.confirmed) + tr(L"\r\nStato: ") + locale.message(wide(r.status));
    if (!r.error.empty())
        out += tr(L"\r\nNota: ") + locale.message(wide(r.error));
    out += tr(L"\r\nTracker nel torrent: ") + std::to_wstring(r.meta.trackers.size()) +
           tr(L" (HTTP/HTTPS e UDP supportati; v2 puro non supportato).");
    return out;
}
void refreshPanel() {
    int tab = TabCtrl_GetCurSel(tabs);
    ShowWindow(panel, tab == 3 ? SW_HIDE : SW_SHOW);
    ShowWindow(helpPane, tab == 3 ? SW_SHOW : SW_HIDE);
    if (tab == 3) {
        InvalidateRect(helpPane, nullptr, TRUE);
        return;
    }
    if (tab == 1) {
        std::wstring s;
        for (const auto &line : current.logs)
            s += wide(line.substr(0, 10)) + locale.message(wide(line.substr(10))) + L"\r\n";
        replaceText(panel, s);
        SendMessage(panel, EM_LINESCROLL, 0, (LPARAM)current.logs.size());
    } else if (tab == 2) {
        auto c = engine->getConfig();
        replaceText(
            panel, tr(L"Profilo selezionato: ") + wide(c.client) + L"\r\n" +
                       std::to_wstring(profileFiles.size()) +
                       tr(L" profili client disponibili (qBittorrent, Deluge, Transmission, rTorrent)."
                          L"\r\nImpostazioni: minimo ") +
                       std::to_wstring(c.minimum) + tr(L" KiB/s · massimo ") +
                       std::to_wstring(c.maximum) + L" KiB/s · " + std::to_wstring(c.simultaneous) +
                       tr(L" sessioni · porta dichiarata ") + std::to_wstring(c.port) +
                       tr(L".\r\n¹ Upload annunciato: valore cumulativo inviato nell'ultimo "
                          L"announce riuscito di ciascuna sessione.\r\nI contatori non "
                          L"indicano file realmente caricati su altri peer."));
    } else {
        auto ids = selected();
        if (ids.empty())
            replaceText(
                panel,
                tr(L"Aggiungi o trascina file .torrent per iniziare.\r\nSeleziona una riga "
                   L"per i dettagli. Avvia e Pausa agiscono su tutti se non ci sono righe "
                   L"selezionate.\r\nUpload e velocità sono simulati; gli annunci ai tracker "
                   L"sono reali.\r\nAl riavvio i torrent vengono ripristinati in pausa."));
        else {
            auto i = std::find_if(current.rows.begin(), current.rows.end(),
                                  [&](const TorrentRow &r) { return r.meta.hash == ids.front(); });
            if (i != current.rows.end())
                replaceText(panel, describe(*i));
        }
    }
}
// Tracker in use, or the first one the engine would try.
std::string rowTracker(const TorrentRow &r) {
    if (!r.tracker.empty())
        return r.tracker;
    for (const auto &url : r.meta.trackers)
        if (httpUrl(url) || udpUrl(url))
            return url;
    return r.meta.trackers.empty() ? std::string() : r.meta.trackers.front();
}
void sortRows(std::vector<TorrentRow> &rows) {
    if (sortColumn < 0)
        return;
    auto now = std::chrono::steady_clock::now();
    auto key = [&](const TorrentRow &r) -> double {
        switch (sortColumn) {
        case 2: return (double)r.meta.size;
        case 3: return (double)r.rate;
        case 4: return (double)r.uploaded;
        case 5: return r.meta.size ? (double)r.uploaded / r.meta.size : 0;
        case 6: return r.seeders * 1e7 + r.leechers;
        case 7:
            return r.wanted ? std::chrono::duration<double>(r.due - now).count() : 1e18;
        }
        return 0;
    };
    auto text = [&](const TorrentRow &r) {
        std::wstring t = sortColumn == 1   ? locale.message(wide(r.status))
                         : sortColumn == 8 ? wide(trackerHost(rowTracker(r)))
                                           : wide(r.meta.name);
        CharLowerBuffW(t.data(), (DWORD)t.size());
        return t;
    };
    bool textual = sortColumn == 0 || sortColumn == 1 || sortColumn == 8;
    std::stable_sort(rows.begin(), rows.end(), [&](const TorrentRow &a, const TorrentRow &b) {
        bool less;
        if (textual) {
            auto x = text(a), y = text(b);
            if (x == y)
                return false;
            less = x < y;
        } else {
            auto x = key(a), y = key(b);
            if (x == y)
                return false;
            less = x < y;
        }
        return sortAscending ? less : !less;
    });
}
void updateSortArrow() {
    auto header = ListView_GetHeader(table);
    for (int i = 0; i < columnCount; ++i) {
        HDITEM item = {};
        item.mask = HDI_FORMAT;
        Header_GetItem(header, i, &item);
        item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (i == sortColumn)
            item.fmt |= sortAscending ? HDF_SORTUP : HDF_SORTDOWN;
        Header_SetItem(header, i, &item);
    }
}
void copyToClipboard(HWND w, const std::wstring &text) {
    if (!OpenClipboard(w))
        return;
    EmptyClipboard();
    auto bytes = (text.size() + 1) * sizeof(wchar_t);
    if (auto mem = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        memcpy(GlobalLock(mem), text.c_str(), bytes);
        GlobalUnlock(mem);
        if (!SetClipboardData(CF_UNICODETEXT, mem))
            GlobalFree(mem);
    }
    CloseClipboard();
}
void refresh(bool sample = false) {
    if (!engine)
        return;
    auto ids = selected();
    current = engine->view();
    sortRows(current.rows);
    refreshLock = true;
    SendMessage(table, WM_SETREDRAW, FALSE, 0);
    bool rebuild = visibleIds.size() != current.rows.size();
    if (!rebuild)
        for (size_t i = 0; i < visibleIds.size(); ++i)
            if (visibleIds[i] != current.rows[i].meta.hash) {
                rebuild = true;
                break;
            }
    if (rebuild) {
        ListView_DeleteAllItems(table);
        visibleIds.clear();
        for (size_t i = 0; i < current.rows.size(); ++i) {
            LVITEM item = {};
            item.mask = LVIF_TEXT;
            item.iItem = (int)i;
            item.pszText = (LPWSTR)L"";
            ListView_InsertItem(table, &item);
            visibleIds.push_back(current.rows[i].meta.hash);
            if (std::find(ids.begin(), ids.end(), visibleIds.back()) != ids.end())
                ListView_SetItemState(table, (int)i, LVIS_SELECTED, LVIS_SELECTED);
        }
    }
    for (size_t i = 0; i < current.rows.size(); ++i) {
        const auto &r = current.rows[i];
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(
                       r.due - std::chrono::steady_clock::now())
                       .count();
        wchar_t ratio[48];
        swprintf_s(ratio, L"%.3f", r.meta.size ? (double)r.uploaded / r.meta.size : 0);
        std::wstring cols[] = {wide(r.meta.name),
                               locale.message(wide(r.status)),
                               sizeText(r.meta.size),
                               sizeText(r.rate) + L"/s",
                               sizeText(r.uploaded),
                               ratio,
                               (r.seeders < 0 ? L"—" : std::to_wstring(r.seeders)) + L" / " +
                                   (r.leechers < 0 ? L"—" : std::to_wstring(r.leechers)),
                               r.wanted ? std::to_wstring(std::max(int64_t(0), sec)) + L" s"
                                        : L"—",
                               wide(trackerHost(rowTracker(r)))};
        for (int col = 0; col < columnCount; ++col)
            ListView_SetItemText(table, (int)i, col, cols[col].data());
    }
    SendMessage(table, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(table, nullptr, FALSE);
    refreshLock = false;
    refreshPanel();
    size_t active = 0;
    for (const auto &r : current.rows)
        if (r.wanted)
            ++active;
    std::wstring totals[] = {tr(L"Upload: ") + sizeText(current.speed) + L"/s",
                             tr(L"Storico: ") + sizeText(current.upload),
                             tr(L"Annunciato: ") + sizeText(current.confirmed),
                             std::to_wstring(active) + tr(L" attivi / ") +
                                 std::to_wstring(current.rows.size())};
    if (closing)
        totals[0] = tr(L"Chiusura delle sessioni…");
    for (int i = 0; i < 4; ++i)
        SendMessage(statusbar, SB_SETTEXT, i, (LPARAM)totals[i].c_str());
    updateCommands();
    RECT top = {0, 0, 30000, px(56)};
    InvalidateRect(mainWindow, &top, FALSE);
}
void addPaths(const std::vector<fs::path> &paths) {
    int added = 0, duplicates = 0;
    std::wstring errors;
    for (const auto &p : paths)
        try {
            if (engine->add(p))
                ++added;
            else
                ++duplicates;
        } catch (const std::exception &e) {
            errors += p.filename().wstring() + L": " + wide(e.what()) + L"\r\n";
        }
    refresh();
    if (!errors.empty())
        MessageBox(mainWindow, errors.substr(0, 6000).c_str(), L"Torrent non importati",
                   MB_ICONWARNING);
    if (duplicates)
        replaceText(statusbar, std::to_wstring(added) + L" aggiunti · " +
                                   std::to_wstring(duplicates) + L" duplicati ignorati");
}
void chooseTorrents() {
    std::vector<wchar_t> buffer(65536);
    OPENFILENAME of = {sizeof(of)};
    of.hwndOwner = mainWindow;
    std::wstring filter = tr(L"File torrent");
    filter.push_back(0);
    filter += L"*.torrent";
    filter.append(2, 0);
    of.lpstrFilter = filter.c_str();
    of.lpstrFile = buffer.data();
    of.nMaxFile = (DWORD)buffer.size();
    of.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
               OFN_NOCHANGEDIR;
    if (!GetOpenFileName(&of)) {
        if (CommDlgExtendedError())
            throw std::runtime_error("Selezione file troppo grande o dialogo non disponibile.");
        return;
    }
    std::vector<fs::path> paths;
    fs::path first(buffer.data());
    auto p = buffer.data() + wcslen(buffer.data()) + 1;
    if (!*p)
        paths.push_back(first);
    else
        while (*p) {
            paths.push_back(first / p);
            p += wcslen(p) + 1;
        }
    addPaths(paths);
}
HWND settingsWindow = nullptr;
HFONT settingsFont = nullptr;
HWND control(HWND w, const wchar_t *cls, const wchar_t *label, DWORD style, int x, int y, int width,
             int height, int id = 0) {
    auto translated = tr(label);
    auto h = CreateWindowEx((wcscmp(cls, L"EDIT") == 0) ? WS_EX_CLIENTEDGE : 0, cls,
                            translated.c_str(), WS_CHILD | WS_VISIBLE | style, px(x), px(y),
                            px(width), px(height), w, (HMENU)(INT_PTR)id, appInstance, nullptr);
    SendMessage(h, WM_SETFONT, (WPARAM)uiFont, TRUE);
    return h;
}
void populateSettings(HWND w) {
    auto c = engine->getConfig();
    int vals[] = {c.minimum, c.maximum, c.simultaneous, c.port};
    for (int i = 0; i < 4; ++i)
        SetDlgItemInt(w, 210 + i, vals[i], FALSE);
    wchar_t t[64];
    swprintf_s(t, L"%.3f", c.target);
    SetDlgItemText(w, 214, t);
    CheckDlgButton(w, 215, c.keepZero ? BST_CHECKED : BST_UNCHECKED);
    auto combo = GetDlgItem(w, 216);
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < profileFiles.size(); ++i) {
        auto label = profileFiles[i].stem().wstring();
        if (!supportedProfile(readJson(profileFiles[i])))
            label += tr(L" [solo consultazione]");
        else if (label.rfind(L"rtorrent-", 0) == 0)
            label += tr(L" [sperimentale]");
        int idx = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)label.c_str());
        SendMessage(combo, CB_SETITEMDATA, idx, (LPARAM)i);
        if (utf8(profileFiles[i].filename().wstring()) == c.client)
            SendMessage(combo, CB_SETCURSEL, idx, 0);
    }
}
json settingsJson(HWND w) {
    json j;
    const char *keys[] = {"minUploadRate", "maxUploadRate", "simultaneousSeed", "port"};
    for (int i = 0; i < 4; ++i) {
        auto s = utf8(readText(GetDlgItem(w, 210 + i)));
        int v = 0;
        auto result = std::from_chars(s.data(), s.data() + s.size(), v);
        if (result.ec != std::errc() || result.ptr != s.data() + s.size())
            throw std::runtime_error("Inserire valori interi validi.");
        j[keys[i]] = v;
    }
    auto s = utf8(readText(GetDlgItem(w, 214)));
    std::replace(s.begin(), s.end(), ',', '.');
    double target = 0;
    auto result = std::from_chars(s.data(), s.data() + s.size(), target);
    if (result.ec != std::errc() || result.ptr != s.data() + s.size() || !std::isfinite(target))
        throw std::runtime_error("Ratio non valido.");
    j["uploadRatioTarget"] = target;
    j["keepTorrentWithZeroLeechers"] = IsDlgButtonChecked(w, 215) == BST_CHECKED;
    auto combo = GetDlgItem(w, 216);
    auto idx = SendMessage(combo, CB_GETCURSEL, 0, 0);
    if (idx == CB_ERR)
        throw std::runtime_error("Selezionare un profilo.");
    auto pos = SendMessage(combo, CB_GETITEMDATA, idx, 0);
    if (pos < 0 || (size_t)pos >= profileFiles.size())
        throw std::runtime_error("Profilo non valido.");
    j["client"] = utf8(profileFiles[pos].filename().wstring());
    return j;
}
LRESULT CALLBACK settingsProc(HWND w, UINT m, WPARAM a, LPARAM b) {
    try {
        switch (m) {
        case WM_CREATE: {
            control(w, L"STATIC", L"Impostazioni sessione", 0, 24, 18, 580, 30);
            control(w, L"STATIC", L"Metti in pausa tutte le sessioni prima di applicare modifiche.",
                    0, 24, 52, 600, 22);
            const wchar_t *labels[] = {L"Upload minimo (KiB/s)", L"Upload massimo (KiB/s)",
                                       L"Torrent simultanei", L"Porta dichiarata al tracker",
                                       L"Ratio obiettivo (-1 = illimitato)"};
            for (int i = 0; i < 5; ++i) {
                control(w, L"STATIC", labels[i], 0, 24, 94 + 42 * i, 310, 26);
                control(w, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, 360, 92 + 42 * i, 220, 28,
                        210 + i);
            }
            control(w, L"STATIC", L"Profilo client", 0, 24, 307, 300, 24);
            control(w, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 24, 337, 556,
                    300, 216);
            control(w, L"BUTTON", L"Mantieni torrent senza leecher (velocità zero)",
                    WS_TABSTOP | BS_AUTOCHECKBOX, 24, 378, 556, 26, 215);
            control(w, L"BUTTON", L"Chiudendo con X riduci nell'area di notifica",
                    WS_TABSTOP | BS_AUTOCHECKBOX, 24, 408, 556, 26, 220);
            CheckDlgButton(w, 220, closeToTray ? BST_CHECKED : BST_UNCHECKED);
            control(w, L"BUTTON", L"Importa configurazione…", WS_TABSTOP, 24, 458, 180, 34, 217);
            control(w, L"BUTTON", L"Applica", WS_TABSTOP | BS_DEFPUSHBUTTON, 370, 458, 100, 34,
                    218);
            control(w, L"BUTTON", L"Chiudi", WS_TABSTOP, 480, 458, 100, 34, 219);
            populateSettings(w);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(a) == 220 && HIWORD(a) == BN_CLICKED) {
                // Window behaviour, applied at once: no need to pause sessions.
                closeToTray = IsDlgButtonChecked(w, 220) == BST_CHECKED;
                saveUiPreferences();
                InvalidateRect(helpPane, nullptr, FALSE);
                return 0;
            }
            if (LOWORD(a) == 219) {
                DestroyWindow(w);
                return 0;
            }
            if (LOWORD(a) == 218) {
                engine->setConfig(settingsJson(w));
                refresh();
                MessageBox(w, L"Impostazioni salvate.", L"TorrentDesk", MB_OK);
                return 0;
            }
            if (LOWORD(a) == 217) {
                wchar_t path[32768] = {};
                OPENFILENAME of = {sizeof(of)};
                of.hwndOwner = w;
                std::wstring filter = tr(L"Configurazione JSON");
                filter.push_back(0);
                filter += L"*.json";
                filter.append(2, 0);
                of.lpstrFilter = filter.c_str();
                of.lpstrFile = path;
                of.nMaxFile = 32768;
                of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileName(&of)) {
                    engine->setConfig(readJson(path));
                    populateSettings(w);
                    refresh();
                }
                return 0;
            }
            break;
        case WM_CTLCOLORSTATIC: {
            auto dc = (HDC)a;
            SetTextColor(dc, ink);
            SetBkColor(dc, RGB(255, 255, 255));
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        }
        case WM_CLOSE:
            DestroyWindow(w);
            return 0;
        case WM_DESTROY:
            settingsWindow = nullptr;
            EnableWindow(mainWindow, TRUE);
            SetForegroundWindow(mainWindow);
            return 0;
        }
    } catch (const std::exception &e) {
        errorBox(w, e);
        return 0;
    }
    return DefWindowProc(w, m, a, b);
}
void openSettings() {
    if (settingsWindow) {
        SetForegroundWindow(settingsWindow);
        return;
    }
    settingsWindow = CreateWindowEx(
        WS_EX_DLGMODALFRAME, L"TorrentDeskSettings", tr(L"TorrentDesk · Impostazioni").c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, px(630), px(555),
        mainWindow, nullptr, appInstance, nullptr);
    if (settingsWindow) {
        RECT parent;
        GetWindowRect(mainWindow, &parent);
        SetWindowPos(settingsWindow, nullptr,
                     parent.left + std::max<LONG>(0, (parent.right - parent.left - px(630)) / 2),
                     parent.top + std::max<LONG>(0, (parent.bottom - parent.top - px(530)) / 2), 0,
                     0, SWP_NOSIZE | SWP_NOZORDER);
        EnableWindow(mainWindow, FALSE);
        ShowWindow(settingsWindow, SW_SHOW);
    }
}

void setArtwork() {
    auto oldImages = std::move(commandImages);
    auto oldTab = tabImages;
    for (int i = 0; i < 6; ++i) {
        auto images = ImageList_Create(px(16), px(16), ILC_COLOR32, 1, 0);
        auto icon = (HICON)LoadImage(appInstance, MAKEINTRESOURCE(201 + i), IMAGE_ICON, px(16),
                                     px(16), LR_SHARED);
        ImageList_AddIcon(images, icon);
        commandImages.push_back(images);
        BUTTON_IMAGELIST image = {};
        image.himl = images;
        image.margin = {px(4), 0, px(2), 0};
        image.uAlign = BUTTON_IMAGELIST_ALIGN_LEFT;
        SendMessage(GetDlgItem(mainWindow, 100 + i), BCM_SETIMAGELIST, 0, (LPARAM)&image);
    }
    tabImages = ImageList_Create(px(16), px(16), ILC_COLOR32, 1, 0);
    ImageList_AddIcon(tabImages, (HICON)LoadImage(appInstance, MAKEINTRESOURCE(207), IMAGE_ICON,
                                                  px(16), px(16), LR_SHARED));
    TabCtrl_SetImageList(tabs, tabImages);
    TCITEM helpItem = {};
    helpItem.mask = TCIF_IMAGE;
    helpItem.iImage = 0;
    TabCtrl_SetItem(tabs, 3, &helpItem);

    for (auto images : oldImages)
        ImageList_Destroy(images);
    if (oldTab)
        ImageList_Destroy(oldTab);
}
bool ensureTray() {
    if (trayPresent)
        return true;
    trayData = {sizeof(trayData)};
    trayData.hWnd = mainWindow;
    trayData.uID = 1;
    trayData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    trayData.uCallbackMessage = trayCallback;
    trayData.hIcon = appIcon;
    auto tip = tr(L"TorrentDesk - attivo in background");
    wcsncpy_s(trayData.szTip, tip.c_str(), _TRUNCATE);
    trayPresent = Shell_NotifyIcon(NIM_ADD, &trayData) != FALSE;
    if (trayPresent) {
        trayData.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIcon(NIM_SETVERSION, &trayData);
    }
    return trayPresent;
}
void restoreWindow() {
    if (mainWindow) {
        ShowWindow(mainWindow, IsIconic(mainWindow) ? SW_RESTORE : SW_SHOW);
        SetForegroundWindow(mainWindow);
    }
}
void hideToTray() {
    if (settingsWindow)
        DestroyWindow(settingsWindow);
    if (ensureTray())
        ShowWindow(mainWindow, SW_HIDE);
    else
        MessageBox(mainWindow, L"Icona tray non disponibile. La finestra resta aperta.",
                   L"TorrentDesk", MB_ICONWARNING);
}
void beginExit() {
    if (settingsWindow)
        DestroyWindow(settingsWindow);
    if (closing)
        return;
    try {
        saveUiPreferences();
    } catch (...) {
    }
    closing = true;
    for (int id = 100; id <= 105; ++id)
        EnableWindow(GetDlgItem(mainWindow, id), FALSE);
    EnableWindow(languageCombo, FALSE);
    DragAcceptFiles(mainWindow, FALSE);
    closer = std::thread([] {
        engine->shutdown();
        closed = true;
    });
    refresh();
}
void applyLanguage() {
    updateCommands();
    const wchar_t *columns[] = {L"Nome",   L"Stato", L"Dimensione",   L"Velocità", L"Upload",
                                L"Ratio",  L"Seed / Leech", L"Announce", L"Tracker"};
    for (int i = 0; i < columnCount; ++i) {
        auto t = tr(columns[i]);
        LVCOLUMN col = {};
        col.mask = LVCF_TEXT;
        col.pszText = t.data();
        ListView_SetColumn(table, i, &col);
    }
    const wchar_t *names[] = {L"Dettagli", L"Registro attività", L"Profili e contatori", L"Aiuto"};
    for (int i = 0; i < 4; ++i) {
        auto t = tr(names[i]);
        TCITEM item = {};
        item.mask = TCIF_TEXT;
        item.pszText = t.data();
        TabCtrl_SetItem(tabs, i, &item);
    }
    if (trayPresent) {
        auto tip = tr(L"TorrentDesk - attivo in background");
        wcsncpy_s(trayData.szTip, tip.c_str(), _TRUNCATE);
        trayData.uFlags = NIF_TIP;
        Shell_NotifyIcon(NIM_MODIFY, &trayData);
    }
    SetWindowText(GetDlgItem(helpPane, 401), tr(L"Documentazione").c_str());
    SetWindowText(GetDlgItem(helpPane, 402), tr(L"Licenze").c_str());
    layout();
    refresh();
}
LRESULT CALLBACK helpProc(HWND w, UINT m, WPARAM a, LPARAM b) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        auto dc = BeginPaint(w, &ps);
        RECT r;
        GetClientRect(w, &r);
        FillRect(dc, &r, backgroundBrush);
        DrawIconEx(dc, px(8), px(10), appLogoIcon, px(112), px(112), 0, nullptr, DI_NORMAL);
        text(dc, std::wstring(L"TorrentDesk ") + TORRENTDESK_VERSION, {px(144), px(5), r.right, px(38)},
             titleFont, ink);
        auto info = tr(L"Applicazione didattica C++ / Win32 per Windows.") + L"\n" +
                    tr(L"Upload simulato e annunci HTTP/HTTPS. Nessun trasferimento P2P.") + L"\n" +
                    tr(L"Licenza Apache-2.0. JSON: nlohmann/json, licenza MIT.") + L"\n" +
                    tr(closeToTray ? L"X nasconde nella tray (modificabile in Impostazioni). Per "
                                     L"chiudere: menu tray > Esci."
                                   : L"X chiude il programma (modificabile in Impostazioni).");
        text(dc, info, {px(144), px(43), r.right - px(8), px(145)}, uiFont, muted,
             DT_LEFT | DT_WORDBREAK);
        EndPaint(w, &ps);
        return 0;
    }
    if (m == WM_CREATE) {
        control(w, L"BUTTON", L"Documentazione", WS_TABSTOP, 144, 150, 155, 30, 401);
        control(w, L"BUTTON", L"Licenze", WS_TABSTOP, 310, 150, 110, 30, 402);
        return 0;
    }
    if (m == WM_COMMAND && (LOWORD(a) == 401 || LOWORD(a) == 402)) {
        auto lang = locale.code() == "it" ? L"it" : L"en";
        auto path = base / L"docs" /
                    (std::wstring(LOWORD(a) == 402 ? L"licenses." : L"help.") + lang + L".html");
        ShellExecute(w, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return 0;
    }
    if (m == WM_ERASEBKGND)
        return 1;
    return DefWindowProc(w, m, a, b);
}
LRESULT CALLBACK proc(HWND w, UINT m, WPARAM a, LPARAM b) {
    try {
        if (m == taskbarCreated && taskbarCreated) {
            bool hadTray = trayPresent;
            trayPresent = false;
            if (hadTray && !ensureTray())
                restoreWindow();
            return 0;
        }
        if (m == restoreMessage && restoreMessage) {
            if (a == 1)
                beginExit();
            else
                restoreWindow();
            return 0;
        }
        switch (m) {
        case WM_CREATE: {
            mainWindow = w;
            dpi = GetDpiForWindow(w);
            setFonts();
            for (int i = 0; i < 6; ++i)
                control(w, L"BUTTON", commandLabel(i, false), WS_TABSTOP | BS_PUSHBUTTON, 0, 0,
                        100, 35, 100 + i);
            table =
                CreateWindowEx(0, WC_LISTVIEW, L"Torrent",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
                               0, 0, 100, 100, w, (HMENU)120, appInstance, nullptr);
            ListView_SetExtendedListViewStyle(table, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER |
                                                         LVS_EX_LABELTIP | LVS_EX_HEADERDRAGDROP);
            SetWindowTheme(table, L"Explorer", nullptr);
            ListView_SetBkColor(table, RGB(255, 255, 255));
            ListView_SetTextBkColor(table, RGB(255, 255, 255));
            ListView_SetTextColor(table, ink);
            const wchar_t *columns[] = {L"Nome",  L"Stato",        L"Dimensione", L"Velocità",
                                        L"Upload", L"Ratio",       L"Seed / Leech", L"Announce",
                                        L"Tracker"};
            int widths[] = {230, 125, 90, 95, 95, 65, 80, 75, 120};
            userWidths = savedWidths.size() == (size_t)columnCount;
            for (int i = 0; i < columnCount; ++i) {
                LVCOLUMN c = {};
                c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
                auto label = tr(columns[i]);
                c.pszText = label.data();
                c.cx = px(userWidths ? std::clamp(savedWidths[i], 30, 1200) : widths[i]);
                c.fmt = i > 1 && i < 8 ? LVCFMT_RIGHT : LVCFMT_LEFT;
                ListView_InsertColumn(table, i, &c);
            }
            updateSortArrow();
            tabs = CreateWindowEx(0, WC_TABCONTROL, L"Dettagli", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                  0, 0, 100, 30, w, (HMENU)121, appInstance, nullptr);
            for (auto s : {L"Dettagli", L"Registro attività", L"Profili e contatori", L"Aiuto"}) {
                TCITEM t = {};
                t.mask = TCIF_TEXT;
                auto label = tr(s);
                t.pszText = label.data();
                TabCtrl_InsertItem(tabs, TabCtrl_GetItemCount(tabs), &t);
            }
            panel = control(w, L"EDIT", L"",
                            WS_TABSTOP | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                            0, 0, 100, 100, 122);
            statusbar =
                CreateWindowEx(0, STATUSCLASSNAME, L"", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0,
                               0, 100, 20, w, nullptr, appInstance, nullptr);
            helpPane = CreateWindowEx(0, L"TorrentDeskHelp", L"", WS_CHILD, 0, 0, 100, 100, w,
                                      nullptr, appInstance, nullptr);
            languageCombo = control(w, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                    0, 0, 140, 250, 130);
            for (const auto &entry : fs::directory_iterator(base / L"languages"))
                if (entry.path().extension() == L".json") {
                    try {
                        auto catalog = readJson(entry.path());
                        auto code = entry.path().stem().string();
                        if (!std::regex_match(code, std::regex("[a-z]{2}(-[A-Z]{2})?")))
                            continue;
                        auto label = wide(catalog.at("name").get<std::string>());
                        languageChoices.push_back({code, label});
                        int index =
                            (int)SendMessage(languageCombo, CB_ADDSTRING, 0, (LPARAM)label.c_str());
                        if (code == locale.code())
                            SendMessage(languageCombo, CB_SETCURSEL, index, 0);
                    } catch (...) {
                    }
                }
            setArtwork();
            createTooltips(w);
            EnumChildWindows(
                w,
                [](HWND h, LPARAM) -> BOOL {
                    SendMessage(h, WM_SETFONT, (WPARAM)uiFont, TRUE);
                    return TRUE;
                },
                0);
            DragAcceptFiles(w, TRUE);
            SetTimer(w, 1, 1000, nullptr);
            refresh();
            return 0;
        }
        case WM_SIZE:
            if (table)
                layout();
            return 0;
        case WM_DPICHANGED: {
            dpi = HIWORD(a);
            setFonts();
            setArtwork();
            auto r = (RECT *)b;
            SetWindowPos(w, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            layout();
            return 0;
        }
        case WM_SETCURSOR:
            if ((HWND)a == w && LOWORD(b) == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(w, &pt);
                RECT t, tb;
                GetWindowRect(table, &t);
                GetWindowRect(tabs, &tb);
                MapWindowPoints(nullptr, w, (POINT *)&t, 2);
                MapWindowPoints(nullptr, w, (POINT *)&tb, 2);
                if (pt.y >= t.bottom && pt.y < tb.top) {
                    SetCursor(LoadCursor(nullptr, IDC_SIZENS));
                    return TRUE;
                }
            }
            break;
        case WM_LBUTTONDOWN: {
            RECT t, tb;
            GetWindowRect(table, &t);
            GetWindowRect(tabs, &tb);
            MapWindowPoints(nullptr, w, (POINT *)&t, 2);
            MapWindowPoints(nullptr, w, (POINT *)&tb, 2);
            int y = (short)HIWORD(b);
            if (y >= t.bottom && y < tb.top) {
                draggingSplitter = true;
                SetCapture(w);
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if (draggingSplitter) {
                RECT r;
                GetClientRect(w, &r);
                int y = (short)HIWORD(b);
                int keep = std::max(px(200), (int)(r.bottom - px(108)) * 2 / 5);
                int maxBottom = MulDiv(r.bottom - px(108) - keep, 96, (int)dpi);
                panelHeight = std::clamp(MulDiv(r.bottom - y, 96, (int)dpi), 150,
                                         std::max(150, maxBottom));
                layout();
            }
            return 0;
        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED:
            if (draggingSplitter) {
                draggingSplitter = false;
                if (m == WM_LBUTTONUP)
                    ReleaseCapture();
                try {
                    saveUiPreferences();
                } catch (...) {
                }
            }
            return 0;
        case WM_GETMINMAXINFO:
            ((MINMAXINFO *)b)->ptMinTrackSize = {px(1000), px(600)};
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            auto dc = BeginPaint(w, &ps);
            paintHeader(dc);
            EndPaint(w, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            auto dc = (HDC)a;
            SetTextColor(dc, muted);
            SetBkColor(dc, bg);
            return (LRESULT)backgroundBrush;
        }
        case WM_NOTIFY: {
            auto h = (NMHDR *)b;
            if (h->code == TTN_GETDISPINFO && h->hwndFrom == tooltip) {
                auto info = (NMTTDISPINFO *)b;
                int id = GetDlgCtrlID((HWND)h->idFrom);
                if (id >= 100 && id <= 105) {
                    tipBuffer = tr(commandTip(id - 100));
                    info->lpszText = tipBuffer.data();
                }
                return 0;
            }
            if (h->hwndFrom == tabs && h->code == TCN_SELCHANGE) {
                layout();
                refreshPanel();
                return 0;
            }
            if (table && h->hwndFrom == ListView_GetHeader(table) &&
                (h->code == HDN_ENDTRACK || h->code == HDN_DIVIDERDBLCLICK)) {
                userWidths = true;
                return 0;
            }
            if (h->hwndFrom == table) {
                if (h->code == LVN_COLUMNCLICK) {
                    int col = ((NMLISTVIEW *)b)->iSubItem;
                    if (col == sortColumn)
                        sortAscending = !sortAscending;
                    else {
                        sortColumn = col;
                        sortAscending = col == 0 || col == 1 || col == 8;
                    }
                    updateSortArrow();
                    try {
                        saveUiPreferences();
                    } catch (...) {
                    }
                    refresh();
                    return 0;
                }
                if (h->code == LVN_KEYDOWN) {
                    auto key = ((NMLVKEYDOWN *)b)->wVKey;
                    if (key == VK_DELETE)
                        PostMessage(w, WM_COMMAND, 103, 0);
                    else if (key == 'A' && (GetKeyState(VK_CONTROL) & 0x8000))
                        ListView_SetItemState(table, -1, LVIS_SELECTED, LVIS_SELECTED);
                    return 0;
                }
                if (h->code == NM_RCLICK) {
                    bool none = ListView_GetSelectedCount(table) == 0;
                    HMENU menu = CreatePopupMenu();
                    auto add = [&](UINT id, const wchar_t *label, bool enabled) {
                        auto t = tr(label);
                        AppendMenu(menu, MF_STRING | (enabled ? 0 : MF_GRAYED), id, t.c_str());
                    };
                    bool any = !current.rows.empty();
                    add(101, commandLabel(1, none), any);
                    add(102, commandLabel(2, none), any);
                    add(103, L"Rimuovi", !none);
                    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
                    add(310, L"Copia nome", !none);
                    add(311, L"Copia infohash", !none);
                    POINT pt;
                    GetCursorPos(&pt);
                    UINT id = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0,
                                             w, nullptr);
                    DestroyMenu(menu);
                    if (id)
                        PostMessage(w, WM_COMMAND, id, 0);
                    return 0;
                }
                if (h->code == LVN_ITEMCHANGED && !refreshLock) {
                    updateCommands();
                    refreshPanel();
                    return 0;
                }
                if (h->code == NM_CUSTOMDRAW) {
                    auto d = (NMLVCUSTOMDRAW *)b;
                    if (d->nmcd.dwDrawStage == CDDS_PREPAINT)
                        return CDRF_NOTIFYITEMDRAW;
                    if (d->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                        auto i = (size_t)d->nmcd.dwItemSpec;
                        d->clrTextBk = i % 2 ? RGB(248, 250, 251) : RGB(255, 255, 255);
                        // Red: announce errors. Amber: accepted with a tracker warning.
                        d->clrText = i >= current.rows.size()         ? ink
                                     : current.rows[i].errors > 0     ? RGB(164, 63, 40)
                                     : !current.rows[i].error.empty() ? RGB(150, 95, 0)
                                                                      : ink;
                        return CDRF_DODEFAULT;
                    }
                }
            }
            break;
        }
        case WM_TIMER:
            if (uiSmoke && !closing) {
                if (uiStage == 0) {
                    ShowWindow(w, SW_SHOW);
                    for (size_t i = 0; i < languageChoices.size(); ++i)
                        if (languageChoices[i].first == "en") {
                            SendMessage(languageCombo, CB_SETCURSEL, i, 0);
                            SendMessage(w, WM_COMMAND, MAKEWPARAM(130, CBN_SELCHANGE),
                                        (LPARAM)languageCombo);
                            break;
                        }
                    uiResults["languageSwitch"] =
                        locale.code() == "en" && readText(GetDlgItem(w, 101)) == L"Start all";
                    uiResults["languageSaved"] =
                        fs::exists(dataDir / L"ui.json") &&
                        readJson(dataDir / L"ui.json").value("language", "") == "en";
                    TabCtrl_SetCurSel(tabs, 3);
                    layout();
                    refreshPanel();
                    uiResults["helpTab"] = IsWindowVisible(helpPane) &&
                                           readText(GetDlgItem(helpPane, 401)) == L"Documentation";
                } else if (uiStage == 1) {
                    uiResults["trayCreated"] = ensureTray();
                    if (trayPresent)
                        SendMessage(w, WM_CLOSE, 0, 0);
                    uiResults["closeHides"] = trayPresent && !IsWindowVisible(w);
                } else if (uiStage == 2) {
                    SendMessage(w, trayCallback, 0, MAKELPARAM(NIN_SELECT, 1));
                    uiResults["trayRestores"] = IsWindowVisible(w) != FALSE;
                } else if (uiStage == 3) {
                    bool passed = true;
                    for (const auto &item : uiResults.items())
                        if (!item.value().get<bool>())
                            passed = false;
                    uiTestExit = passed ? 0 : 1;
                    uiResults["passed"] = passed;
                    {
                        std::ofstream out(dataDir / L"ui-smoke.json");
                        out << uiResults.dump(2);
                    }
                    SendMessage(w, WM_COMMAND, 305, 0);
                }
                ++uiStage;
            }
            if (closing && closed) {
                DestroyWindow(w);
                return 0;
            }
            refresh(true);
            return 0;
        case WM_COMMAND:
            if (closing)
                return 0;
            if (LOWORD(a) == 130 && HIWORD(a) == CBN_SELCHANGE) {
                int index = (int)SendMessage(languageCombo, CB_GETCURSEL, 0, 0);
                if (index >= 0 && (size_t)index < languageChoices.size()) {
                    auto old = locale.code();
                    try {
                        locale.load(base, languageChoices[index].first);
                        saveUiPreferences();
                        applyLanguage();
                    } catch (...) {
                        locale.load(base, old);
                        for (size_t i = 0; i < languageChoices.size(); ++i)
                            if (languageChoices[i].first == old)
                                SendMessage(languageCombo, CB_SETCURSEL, i, 0);
                        throw;
                    }
                }
                return 0;
            }
            switch (LOWORD(a)) {
            case 100:
                chooseTorrents();
                break;
            case 101:
                engine->command(selected(), 1);
                refresh();
                break;
            case 102:
                engine->command(selected(), 2);
                refresh();
                break;
            case 103: {
                auto ids = selected();
                if (ids.empty()) {
                    MessageBox(w, L"Seleziona i torrent da rimuovere.", L"TorrentDesk", MB_OK);
                    break;
                }
                if (MessageBox(w, L"Rimuovere i torrent selezionati?", L"TorrentDesk",
                               MB_YESNO | MB_ICONQUESTION) != IDYES)
                    break;
                engine->command(ids, 3);
                refresh();
                break;
            }
            case 104:
                openSettings();
                break;
            case 310:
            case 311: {
                std::wstring text;
                for (int i = -1; (i = ListView_GetNextItem(table, i, LVNI_SELECTED)) != -1;)
                    if ((size_t)i < current.rows.size())
                        text += (text.empty() ? L"" : L"\r\n") +
                                wide(LOWORD(a) == 310 ? current.rows[i].meta.name
                                                      : hex(current.rows[i].meta.hash));
                if (!text.empty())
                    copyToClipboard(w, text);
                break;
            }
            case 105:
                hideToTray();
                break;
            case 301:
                restoreWindow();
                break;
            case 302:
                engine->command({}, 1);
                refresh();
                break;
            case 303:
                engine->command({}, 2);
                refresh();
                break;
            case 304:
                restoreWindow();
                TabCtrl_SetCurSel(tabs, 3);
                layout();
                refreshPanel();
                break;
            case 305:
                beginExit();
                break;
            }
            return 0;
        case WM_DROPFILES: {
            HDROP drop = (HDROP)a;
            std::vector<fs::path> paths;
            UINT n = DragQueryFile(drop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < n; ++i) {
                UINT len = DragQueryFile(drop, i, nullptr, 0);
                std::wstring p(len + 1, 0);
                DragQueryFile(drop, i, p.data(), len + 1);
                p.resize(len);
                paths.emplace_back(p);
            }
            DragFinish(drop);
            addPaths(paths);
            return 0;
        }
        case WM_CLOSE:
            if (!closing) {
                if (closeToTray)
                    hideToTray();
                else
                    beginExit();
            }
            return 0;
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_ENDSESSION:
            if (a)
                beginExit();
            return 0;
        case trayCallback: {
            UINT event = LOWORD(b);
            if (event == NIN_SELECT || event == NIN_KEYSELECT || event == WM_LBUTTONDBLCLK)
                restoreWindow();
            if (event == WM_CONTEXTMENU || event == WM_RBUTTONUP) {
                HMENU menu = CreatePopupMenu();
                auto add = [&](UINT id, const wchar_t *label) {
                    auto t = tr(label);
                    AppendMenu(menu, MF_STRING, id, t.c_str());
                };
                add(301, L"Apri TorrentDesk");
                AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
                add(302, L"Avvia tutti");
                add(303, L"Pausa tutti");
                add(304, L"Aiuto");
                AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
                add(305, L"Esci");
                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(w);
                UINT id = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, w,
                                         nullptr);
                DestroyMenu(menu);
                PostMessage(w, WM_NULL, 0, 0);
                if (id)
                    PostMessage(w, WM_COMMAND, id, 0);
            }
            return 0;
        }
        case WM_DESTROY:
            if (trayPresent)
                Shell_NotifyIcon(NIM_DELETE, &trayData);
            for (auto images : commandImages)
                ImageList_Destroy(images);
            if (tabImages)
                ImageList_Destroy(tabImages);
            KillTimer(w, 1);
            PostQuitMessage(uiTestExit);
            return 0;
        }
    } catch (const std::exception &e) {
        errorBox(w, e);
        return 0;
    }
    return DefWindowProc(w, m, a, b);
}
int WINAPI wWinMain(HINSTANCE h, HINSTANCE, LPWSTR, int n) {
    try {
        appInstance = h;
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        INITCOMMONCONTROLSEX init = {sizeof(init),
                                     ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_STANDARD_CLASSES};
        InitCommonControlsEx(&init);
        wchar_t path[32768];
        if (!GetModuleFileName(nullptr, path, 32768))
            return 1;
        base = fs::path(path).parent_path();
        int argc;
        auto argv = CommandLineToArgvW(GetCommandLine(), &argc);
        std::vector<fs::path> inputs;
        bool requestQuit = false;
        for (int i = 1; i < argc; ++i) {
            if (std::wstring(argv[i]) == L"--data-dir" && i + 1 < argc)
                dataDir = argv[++i];
            else if (std::wstring(argv[i]) == L"--ui-test")
                uiSmoke = true;
            else if (std::wstring(argv[i]) == L"--quit")
                requestQuit = true;
            else
                inputs.emplace_back(argv[i]);
        }
        LocalFree(argv);
        if (dataDir.empty() && fs::exists(base / L"portable.flag"))
            dataDir = base / L"data";
        if (dataDir.empty()) {
            PWSTR local = nullptr;
            if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local)))
                throw std::runtime_error("Cartella dati non disponibile.");
            dataDir = fs::path(local) / L"TorrentDesk";
            CoTaskMemFree(local);
        }
        fs::create_directories(dataDir);
        std::string language =
            PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_ITALIAN ? "it" : "en";
        try {
            if (fs::exists(dataDir / L"ui.json")) {
                auto ui = readJson(dataDir / L"ui.json");
                language = ui.value("language", language);
                closeToTray = ui.value("closeToTray", true);
                sortColumn = std::clamp(ui.value("sortColumn", -1), -1, columnCount - 1);
                sortAscending = ui.value("sortAscending", true);
                panelHeight = std::clamp(ui.value("panelHeight", 185), 150, 2000);
                if (ui.contains("columnWidths") && ui["columnWidths"].is_array())
                    for (const auto &v : ui["columnWidths"])
                        if (v.is_number_integer())
                            savedWidths.push_back(v.get<int>());
            }
        } catch (...) {
        }
        try {
            locale.load(base, language);
        } catch (...) {
            locale.load(base, "en");
        }
        taskbarCreated = RegisterWindowMessage(L"TaskbarCreated");
        auto mutexName =
            L"Local\\TorrentDesk-" + wide(hex(sha1(utf8(fs::absolute(dataDir).wstring()))));
        restoreMessage = RegisterWindowMessage((L"TorrentDesk.Restore." + mutexName).c_str());
        if (requestQuit) {
            SendNotifyMessage(HWND_BROADCAST, restoreMessage, 1, 0);
            return 0;
        }
        HANDLE single = CreateMutex(nullptr, TRUE, mutexName.c_str());
        if (!single || GetLastError() == ERROR_ALREADY_EXISTS) {
            if (single)
                CloseHandle(single);
            SendNotifyMessage(HWND_BROADCAST, restoreMessage, 0, 0);
            return 0;
        }
        for (const auto &e : fs::directory_iterator(base / L"profiles"))
            if (e.path().extension() == L".client")
                profileFiles.push_back(e.path());
        std::sort(profileFiles.begin(), profileFiles.end());
        engine = std::make_unique<DeskEngine>(dataDir, base / L"profiles");
        appIcon =
            (HICON)LoadImage(h, MAKEINTRESOURCE(101), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
        appLogoIcon = (HICON)LoadImage(h, MAKEINTRESOURCE(101), IMAGE_ICON, 256, 256, LR_SHARED);
        backgroundBrush = CreateSolidBrush(bg);
        WNDCLASSEX wc = {sizeof(wc)};
        wc.hInstance = h;
        wc.lpfnWndProc = proc;
        wc.lpszClassName = L"TorrentDeskMain";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = appIcon;
        wc.hIconSm = appIcon;
        wc.hbrBackground = backgroundBrush;
        RegisterClassEx(&wc);
        WNDCLASS sc = {};
        sc.hInstance = h;
        sc.lpfnWndProc = settingsProc;
        sc.lpszClassName = L"TorrentDeskSettings";
        sc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        sc.hCursor = wc.hCursor;
        sc.hIcon = appIcon;
        RegisterClass(&sc);
        WNDCLASS hc = {};
        hc.hInstance = h;
        hc.lpfnWndProc = helpProc;
        hc.lpszClassName = L"TorrentDeskHelp";
        hc.hCursor = wc.hCursor;
        hc.hbrBackground = backgroundBrush;
        RegisterClass(&hc);
        HWND w = CreateWindowEx(0, wc.lpszClassName, L"TorrentDesk " TORRENTDESK_VERSION,
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                                1120, 740, nullptr, nullptr, h, nullptr);
        if (!w)
            throw std::runtime_error("Creazione finestra fallita.");
        ShowWindow(w, n);
        if (!inputs.empty())
            addPaths(inputs);
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0) > 0) {
            HWND dialog = settingsWindow ? settingsWindow : w;
            if (!IsDialogMessage(dialog, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        if (closer.joinable())
            closer.join();
        engine.reset();
        DeleteObject(uiFont);
        DeleteObject(titleFont);
        DeleteObject(smallFont);
        DeleteObject(backgroundBrush);
        ReleaseMutex(single);
        CloseHandle(single);
        return (int)msg.wParam;
    } catch (const std::exception &e) {
        if (closer.joinable())
            closer.join();
        MessageBox(nullptr, wide(e.what()).c_str(), L"TorrentDesk · Avvio non riuscito",
                   MB_ICONERROR);
        return 1;
    }
}

