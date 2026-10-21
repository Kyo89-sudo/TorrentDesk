#pragma once
#include "torrent.hpp"
#include "text.hpp"
#include "udp.hpp"
#include <winhttp.h>
#include <regex>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <deque>
#include <memory>
#include <iomanip>
#include <sstream>
struct HttpHandle {
    HINTERNET h = nullptr;
    explicit HttpHandle(HINTERNET value) : h(value) {
        if (!h)
            throw std::runtime_error("WinHTTP: errore " + std::to_string(GetLastError()));
    }
    ~HttpHandle() {
        if (h)
            WinHttpCloseHandle(h);
    }
    HttpHandle(const HttpHandle &) = delete;
    operator HINTERNET() const { return h; }
};
// "udp://tracker.example:6969/announce" -> "tracker.example"
inline std::string trackerHost(const std::string &url) {
    auto start = url.find("://");
    start = start == std::string::npos ? 0 : start + 3;
    auto end = url.find_first_of(":/?#", url[start] == '[' ? url.find(']', start) : start);
    return url.substr(start, end == std::string::npos ? std::string::npos : end - start);
}
inline bool httpUrl(const std::string &s) {
    return s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0;
}
inline uint32_t random32() {
    uint32_t v;
    if (BCryptGenRandom(nullptr, (PUCHAR)&v, sizeof(v), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        throw std::runtime_error("Generatore casuale non disponibile.");
    return v;
}
inline std::string encode(const std::string &data, const json &profile) {
    std::regex allowed(profile.at("urlEncoder").at("encodingExclusionPattern").get<std::string>());
    auto digits = profile.at("urlEncoder").value("encodedHexCase", "lower") == "upper"
                      ? "0123456789ABCDEF"
                      : "0123456789abcdef";
    std::string out;
    for (unsigned char c : data) {
        std::string one(1, (char)c);
        if (c < 128 && std::regex_match(one, allowed))
            out += one;
        else {
            out += '%';
            out += digits[c >> 4];
            out += digits[c & 15];
        }
    }
    return out;
}
inline bool supportedProfile(const json &p) {
    try {
        auto peer = p.at("peerIdGenerator").at("algorithm");
        auto key = p.at("keyGenerator").at("algorithm");
        auto type = peer.at("type").get<std::string>();
        if (type == "RANDOM_POOL_WITH_CHECKSUM")
            return key.at("type") == "DIGIT_RANGE_TRANSFORMED_TO_HEX_WITHOUT_LEADING_ZEROES";
        auto pat = peer.at("pattern").get<std::string>();
        return type == "REGEX" &&
               (pat.rfind("-qB", 0) == 0 || pat.rfind("-DE", 0) == 0 ||
                pat.rfind("-lt", 0) == 0) &&
               pat.find("{12}") != std::string::npos &&
               (key.at("type") == "HASH" || key.at("type") == "HASH_NO_LEADING_ZERO");
    } catch (...) {
        return false;
    }
}
inline std::string peerId(const json &p) {
    const auto &a = p.at("peerIdGenerator").at("algorithm");
    std::string out;
    if (a.at("type") == "RANDOM_POOL_WITH_CHECKSUM") {
        out = a.at("prefix");
        auto pool = a.at("charactersPool").get<std::string>();
        int base = a.at("base");
        if (base < 2 || base > (int)pool.size() || out.size() > 18)
            throw std::runtime_error("Profilo peer_id non valido.");
        int sum = 0;
        while (out.size() < 19) {
            int n = (int)(random32() % (uint32_t)base);
            sum += n;
            out += pool[n];
        }
        out += pool[(base - sum % base) % base];
    } else {
        auto pattern = a.at("pattern").get<std::string>();
        auto bracket = pattern.find('[');
        if (bracket == std::string::npos)
            throw std::runtime_error("Profilo peer_id non supportato.");
        out = pattern.substr(0, bracket);
        // rTorrent-style "[\x01-\xff]{12}": arbitrary non-zero bytes (sent URL-encoded).
        // The JSON stores the range as UTF-8, so it is matched textually, not with std::regex.
        if (pattern.compare(bracket, 6, "[\x01-\xc3\xbf]") == 0) {
            while (out.size() < 20)
                out += (char)(1 + random32() % 255);
            if (out.size() != 20)
                throw std::runtime_error("peer_id deve contenere 20 byte.");
            return out;
        }
        const std::string chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_~()!.*-";
        while (out.size() < 20)
            out += chars[random32() % chars.size()];
        if (!std::regex_match(out, std::regex(pattern)))
            throw std::runtime_error("Generazione peer_id non compatibile con il profilo.");
    }
    if (out.size() != 20)
        throw std::runtime_error("peer_id deve contenere 20 byte.");
    return out;
}
inline std::string makeKey(const json &p) {
    auto a = p.at("keyGenerator").at("algorithm");
    std::ostringstream s;
    if (a.at("type") == "DIGIT_RANGE_TRANSFORMED_TO_HEX_WITHOUT_LEADING_ZEROES") {
        auto lo = a.at("inclusiveLowerBound").get<uint32_t>(),
             hi = a.at("inclusiveUpperBound").get<uint32_t>();
        if (hi < lo || hi == UINT32_MAX)
            throw std::runtime_error("Intervallo chiave non valido.");
        s << std::hex << (lo + random32() % (hi - lo + 1));
    } else {
        int n = a.at("length");
        if (n < 1 || n > 64)
            throw std::runtime_error("Lunghezza chiave non valida.");
        for (int i = 0; i < n; ++i)
            s << "0123456789abcdef"[random32() % 16];
    }
    auto v = s.str();
    if (a.at("type") == "HASH_NO_LEADING_ZERO") {
        auto i = v.find_first_not_of('0');
        v = i == std::string::npos ? "0" : v.substr(i);
    }
    if (p.at("keyGenerator").value("keyCase", "lower") == "upper")
        for (auto &c : v)
            c = (char)toupper((unsigned char)c);
    return v;
}
inline std::string announceQuery(const json &p, const TorrentMeta &m, const std::string &peer,
                                 const std::string &key, uint64_t up, const std::string &event,
                                 int port) {
    std::map<std::string, std::string> values = {
        {"infohash", encode(m.hash, p)},
        {"peerid",
         p.at("peerIdGenerator").value("shouldUrlEncode", false) ? encode(peer, p) : peer},
        {"key", encode(key, p)},
        {"uploaded", std::to_string(up)},
        {"downloaded", "0"},
        {"left", "0"},
        {"port", std::to_string(port)},
        {"event", event},
        {"numwant",
         std::to_string(p.at(event == "stopped" ? "numwantOnStop" : "numwant").get<int>())}};
    std::string query = p.at("query"), out;
    std::istringstream stream(query);
    std::string item;
    while (std::getline(stream, item, '&')) {
        if (item.empty())
            continue;
        if (item.find("{ipv6}") != std::string::npos || item.find("{ip}") != std::string::npos ||
            (event.empty() && item.find("{event}") != std::string::npos))
            continue;
        for (const auto &kv : values) {
            std::string token = "{" + kv.first + "}";
            size_t at;
            while ((at = item.find(token)) != std::string::npos)
                item.replace(at, token.size(), kv.second);
        }
        if (item.find('{') != std::string::npos)
            throw std::runtime_error("Placeholder profilo non supportato.");
        if (!out.empty())
            out += '&';
        out += item;
    }
    return out;
}
struct HttpResult {
    std::string body;
    uint64_t received = 0;
};
struct HttpOnce {
    DWORD status = 0;
    std::string location;
    HttpResult result;
};
inline HttpOnce httpGetOnce(const std::string &url, const json &p) {
    auto u = wide(url);
    URL_COMPONENTS c = {sizeof(c)};
    c.dwHostNameLength = c.dwUrlPathLength = c.dwExtraInfoLength = c.dwUserNameLength =
        c.dwPasswordLength = (DWORD)-1;
    if (!WinHttpCrackUrl(u.c_str(), (DWORD)u.size(), 0, &c) ||
        (c.nScheme != INTERNET_SCHEME_HTTP && c.nScheme != INTERNET_SCHEME_HTTPS) ||
        c.dwUserNameLength || c.dwPasswordLength)
        throw std::runtime_error("URL tracker HTTP/HTTPS non valido.");
    std::wstring host(c.lpszHostName, c.dwHostNameLength), path(c.lpszUrlPath, c.dwUrlPathLength);
    if (path.empty())
        path = L"/";
    if (c.dwExtraInfoLength)
        path.append(c.lpszExtraInfo, c.dwExtraInfoLength);
    if (path.find(L'#') != std::wstring::npos)
        throw std::runtime_error("Frammento URL tracker non valido.");
    std::string agent = "TorrentDesk";
    for (const auto &h : p.at("requestHeaders"))
        if (h.at("name") == "User-Agent")
            agent = h.at("value");
    HttpHandle session(WinHttpOpen(wide(agent).c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    WinHttpSetTimeouts(session, 2000, 3000, 4000, 4000);
    HttpHandle connect(WinHttpConnect(session, host.c_str(), c.nPort, 0));
    HttpHandle request(WinHttpOpenRequest(
        connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        c.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0));
    DWORD disabled =
        WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
    WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled));
    DWORD decompress = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
    WinHttpSetOption(request, WINHTTP_OPTION_DECOMPRESSION, &decompress, sizeof(decompress));
    std::wstring headers;
    for (const auto &h : p.at("requestHeaders")) {
        auto name = h.at("name").get<std::string>(), value = h.at("value").get<std::string>();
        if (name == "User-Agent")
            continue;
        if (name.find_first_of("\r\n:") != std::string::npos ||
            value.find_first_of("\r\n") != std::string::npos)
            throw std::runtime_error("Intestazione profilo non valida.");
        headers += wide(name) + L": " + wide(value) + L"\r\n";
    }
    if (!WinHttpSendRequest(request, headers.c_str(), (DWORD)headers.size(),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr))
        throw std::runtime_error("Tracker non raggiungibile (WinHTTP " +
                                 std::to_string(GetLastError()) + ").");
    HttpOnce once;
    DWORD status = 0, len = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &len,
                             WINHTTP_NO_HEADER_INDEX))
        throw std::runtime_error("Risposta tracker non valida.");
    once.status = status;
    if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
        DWORD bytes = 0;
        WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                            WINHTTP_NO_OUTPUT_BUFFER, &bytes, WINHTTP_NO_HEADER_INDEX);
        if (bytes && bytes < 8192) {
            std::wstring loc(bytes / sizeof(wchar_t), 0);
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                                    loc.data(), &bytes, WINHTTP_NO_HEADER_INDEX)) {
                loc.resize(bytes / sizeof(wchar_t));
                once.location = utf8(loc);
            }
        }
        return once;
    }
    if (status != 200)
        throw std::runtime_error("Tracker HTTP " + std::to_string(status) + ".");
    auto &result = once.result;
    char buffer[16384];
    DWORD got = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
    for (;;) {
        if (!WinHttpReadData(request, buffer, sizeof(buffer), &got))
            throw std::runtime_error("Risposta tracker interrotta.");
        if (!got)
            break;
        if (result.body.size() + got > 4 * 1024 * 1024 ||
            std::chrono::steady_clock::now() > deadline)
            throw std::runtime_error("Risposta tracker oltre i limiti.");
        result.body.append(buffer, got);
        result.received += got;
    }
    return once;
}
// Follows up to two redirects. Relative locations stay on the same host; an absolute one
// must be HTTP(S) and may not downgrade HTTPS to HTTP.
inline HttpResult httpGet(const std::string &url, const json &p) {
    std::string current = url;
    for (int hop = 0; hop <= 2; ++hop) {
        auto once = httpGetOnce(current, p);
        if (once.location.empty() && once.status == 200)
            return once.result;
        if (once.location.empty() || hop == 2)
            throw std::runtime_error("Tracker HTTP " + std::to_string(once.status) +
                                     ": troppi redirect o destinazione mancante.");
        std::string next = once.location;
        if (next.find_first_of("\r\n") != std::string::npos)
            throw std::runtime_error("Redirect tracker non valido.");
        if (next[0] == '/') {
            auto scheme = current.find("://");
            auto slash = current.find('/', scheme + 3);
            next = current.substr(0, slash) + next;
        } else if (!httpUrl(next))
            throw std::runtime_error("Redirect tracker non valido.");
        if (current.rfind("https://", 0) == 0 && next.rfind("http://", 0) == 0)
            throw std::runtime_error("Redirect tracker da HTTPS a HTTP rifiutato.");
        current = next;
    }
    throw std::runtime_error("Troppi redirect.");
}
struct TrackerReply {
    int64_t interval = 1800;
    int seeders = -1, leechers = -1;
    std::string warning;
};
inline TrackerReply parseReply(const std::string &bytes, int floor = 30) {
    auto root = BParser(bytes).parse();
    if (root.kind != BValue::Dict)
        throw std::runtime_error("Risposta tracker non valida.");
    if (root.has("failure reason"))
        throw std::runtime_error("Tracker ha rifiutato l'annuncio.");
    TrackerReply r;
    if (root.has("interval")) {
        const auto &v = root.at("interval");
        if (v.kind != BValue::Integer || v.number <= 0 || v.number > 7 * 86400)
            throw std::runtime_error("Intervallo tracker non valido.");
        r.interval = v.number;
    }
    if (root.has("min interval")) {
        const auto &v = root.at("min interval");
        if (v.kind != BValue::Integer || v.number <= 0 || v.number > 7 * 86400)
            throw std::runtime_error("Intervallo minimo tracker non valido.");
        r.interval = (std::max)(r.interval, v.number);
    }
    r.interval = (std::max)(r.interval, (int64_t)floor);
    auto count = [&](const char *k) {
        if (!root.has(k))
            return -1;
        const auto &v = root.at(k);
        if (v.kind != BValue::Integer || v.number < 0 || v.number > INT_MAX)
            throw std::runtime_error("Numero peer non valido.");
        return (int)v.number;
    };
    r.seeders = count("complete");
    r.leechers = count("incomplete");
    if (root.has("warning message"))
        r.warning = "Avviso restituito dal tracker";
    return r;
}
struct DeskConfig {
    int minimum = 30, maximum = 170, simultaneous = 20, port = 49152;
    double target = -1;
    bool keepZero = true;
    std::string client = "qbittorrent-5.2.4.client";
};
struct TorrentRow {
    TorrentMeta meta;
    fs::path source;
    std::string status = "In pausa", error, profileName, tracker;
    bool wanted = false, started = false, busy = false, removing = false;
    uint64_t uploaded = 0, lifetime = 0, confirmed = 0, rate = 0;
    int seeders = -1, leechers = -1, errors = 0;
    std::chrono::steady_clock::time_point due = {};
    json profile;
    std::string peer, key;
};
struct ViewState {
    std::vector<TorrentRow> rows;
    std::vector<std::string> logs;
    uint64_t speed = 0, upload = 0, confirmed = 0, received = 0;
};
class DeskEngine {
    fs::path data, profiles;
    mutable std::mutex mu;
    std::condition_variable cv;
    std::thread worker;
    bool quit = false;
    DeskConfig config;
    std::vector<TorrentRow> rows;
    std::deque<std::string> logs;
    uint64_t received = 0, configRevision = 0;
    int intervalFloor = 30;
    std::map<std::string, std::string> peerCache, keyCache;
    void log(const std::string &s) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        char b[24];
        sprintf_s(b, "%02d:%02d:%02d  ", t.wHour, t.wMinute, t.wSecond);
        logs.push_back(std::string(b) + s);
        if (logs.size() > 300)
            logs.pop_front();
    }
    void saveLocked() {
        json state;
        state["config"] = {{"minUploadRate", config.minimum},
                           {"maxUploadRate", config.maximum},
                           {"simultaneousSeed", config.simultaneous},
                           {"client", config.client},
                           {"keepTorrentWithZeroLeechers", config.keepZero},
                           {"uploadRatioTarget", config.target},
                           {"port", config.port}};
        state["keys"] = keyCache;
        state["torrents"] = json::array();
        for (const auto &r : rows)
            if (!r.removing)
                state["torrents"].push_back({{"hash", hex(r.meta.hash)}, {"lifetime", r.lifetime}});
        auto temp = data / L"state.tmp", path = data / L"state.json";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            auto s = state.dump(2);
            if (!out.write(s.data(), (std::streamsize)s.size()) || !out.flush())
                throw std::runtime_error("Salvataggio sessione fallito.");
        }
        if (!MoveFileEx(temp.c_str(), path.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Impossibile sostituire il file sessione (Windows " + std::to_string(GetLastError()) + ").");
    }
    void tick() {
        using Clock = std::chrono::steady_clock;
        auto last = Clock::now();
        auto lastSave = last;
        auto nextSpeed = last;
        uint64_t totalRate = 0, seenRevision = UINT64_MAX;
        auto shutdownDeadline = Clock::time_point::max();
        for (;;) {
            TorrentRow task;
            bool work = false;
            std::string event;
            int port = 0;
            {
                std::unique_lock<std::mutex> lock(mu);
                cv.wait_for(lock, std::chrono::milliseconds(200));
                auto now = Clock::now();
                double elapsed = std::chrono::duration<double>(now - last).count();
                last = now;
                if (quit) {
                    if (shutdownDeadline == Clock::time_point::max())
                        shutdownDeadline = now + std::chrono::seconds(12);
                    for (auto &r : rows)
                        r.wanted = false;
                    if (now > shutdownDeadline) {
                        for (auto &r : rows) {
                            r.started = false;
                            r.rate = 0;
                        }
                        log("Chiusura: tempo limite per gli stopped raggiunto.");
                        break;
                    }
                }
                size_t active = 0;
                for (auto &r : rows)
                    if (r.wanted && r.started)
                        ++active;
                if (now >= nextSpeed || seenRevision != configRevision) {
                    seenRevision = configRevision;
                    totalRate = ((uint64_t)config.minimum +
                                 random32() % ((uint64_t)config.maximum - config.minimum + 1)) *
                                1024;
                    nextSpeed = now + std::chrono::seconds(60);
                }
                size_t earning = 0;
                for (const auto &r : rows)
                    if (r.started && r.wanted && r.errors == 0 && r.leechers != 0)
                        ++earning;
                for (auto &r : rows) {
                    r.rate = (r.started && r.wanted && r.errors == 0 && r.leechers != 0 && earning)
                                 ? totalRate / earning
                                 : 0;
                    auto delta = (uint64_t)(r.rate * elapsed);
                    auto room = UINT64_MAX - (std::max)(r.uploaded, r.lifetime);
                    delta = (std::min)(delta, room);
                    r.uploaded += delta;
                    r.lifetime += delta;
                    if (config.target >= 0 && r.started && r.wanted &&
                        (long double)r.uploaded >= (long double)r.meta.size * config.target) {
                        r.wanted = false;
                        r.rate = 0;
                        log("Ratio obiettivo raggiunto: " + r.meta.name);
                    }
                }
                for (auto &r : rows) {
                    if (r.busy)
                        continue;
                    if (!r.wanted && r.started) {
                        task = r;
                        event = "stopped";
                        work = true;
                    } else if (r.wanted && !r.started && active < (size_t)config.simultaneous &&
                               now >= r.due) {
                        task = r;
                        event = "started";
                        work = true;
                    } else if (r.wanted && r.started && now >= r.due) {
                        task = r;
                        work = true;
                    }
                    if (work) {
                        r.busy = true;
                        r.status = event == "stopped" ? "Arresto in corso" : "Annuncio in corso";
                        port = config.port;
                        break;
                    }
                }
                if (!work) {
                    for (auto i = rows.begin(); i != rows.end();)
                        if (i->removing && !i->started) {
                            std::error_code ec;
                            fs::remove(i->source, ec);
                            i = rows.erase(i);
                        } else
                            ++i;
                    if (quit)
                        break;
                }
                if (now - lastSave > std::chrono::seconds(15)) {
                    try {
                        saveLocked();
                    } catch (const std::exception &e) {
                        log(e.what());
                    }
                    lastSave = now;
                }
            }
            if (!work)
                continue;
            bool ok = false;
            std::string switched;
            TrackerReply reply;
            std::string error;
            uint64_t responseBytes = 0;
            try {
                if (event == "started") {
                    task.profile = readJson(profiles / fs::u8path(task.profileName));
                    if (!supportedProfile(task.profile))
                        throw std::runtime_error("Profilo non supportato dal motore nativo.");
                    {
                        std::lock_guard<std::mutex> cacheLock(mu);
                        auto cacheId = task.profileName + ":" + hex(task.meta.hash);
                        auto refresh =
                            task.profile.at("peerIdGenerator").at("refreshOn").get<std::string>();
                        if (refresh == "NEVER" || refresh == "TORRENT_PERSISTENT") {
                            auto &peer =
                                peerCache[refresh == "NEVER" ? task.profileName : cacheId];
                            if (peer.empty())
                                peer = peerId(task.profile);
                            task.peer = peer;
                        } else
                            task.peer = peerId(task.profile);
                        if (task.profile.at("keyGenerator").at("refreshOn") ==
                            "TORRENT_PERSISTENT") {
                            auto &key = keyCache[cacheId];
                            if (key.empty())
                                key = makeKey(task.profile);
                            task.key = key;
                        } else
                            task.key = makeKey(task.profile);
                    }
                    task.uploaded = 0;
                    task.confirmed = 0;
                    task.tracker.clear();
                }
                // Failover: the current tracker first, then the others in torrent order
                // (announce, then announce-list tiers). At most 4 per attempt to bound delays.
                std::vector<std::string> candidates;
                if (!task.tracker.empty())
                    candidates.push_back(task.tracker);
                for (const auto &url : task.meta.trackers)
                    if ((httpUrl(url) || udpUrl(url)) && url != task.tracker)
                        candidates.push_back(url);
                if (candidates.empty())
                    throw std::runtime_error("Nessun tracker HTTP/HTTPS o UDP nel torrent.");
                if (candidates.size() > 4)
                    candidates.resize(4);
                std::string lastError;
                for (const auto &url : candidates) {
                    try {
                        reply = announceTo(task, url, event, port, responseBytes);
                        if (!task.tracker.empty() && url != task.tracker)
                            switched = url;
                        task.tracker = url;
                        ok = true;
                        break;
                    } catch (const std::exception &e) {
                        lastError = e.what();
                    }
                }
                if (!ok)
                    throw std::runtime_error(lastError);
            } catch (const std::exception &e) {
                error = e.what();
            }
            {
                std::lock_guard<std::mutex> lock(mu);
                auto it = std::find_if(rows.begin(), rows.end(), [&](const TorrentRow &r) {
                    return r.meta.hash == task.meta.hash;
                });
                if (it == rows.end())
                    continue;
                auto &r = *it;
                r.busy = false;
                received += responseBytes;
                if (ok) {
                    r.tracker = task.tracker;
                    if (!switched.empty())
                        log("Tracker alternativo per " + r.meta.name + ": " + trackerHost(switched));
                    r.errors = 0;
                    r.error = reply.warning;
                    if (event == "started") {
                        r.started = true;
                        r.profile = task.profile;
                        r.peer = task.peer;
                        r.key = task.key;
                        r.tracker = task.tracker;
                        r.uploaded = 0;
                        r.confirmed = 0;
                    } else
                        r.confirmed = task.uploaded;
                    if (event == "stopped") {
                        r.started = false;
                        r.status = "In pausa";
                        r.rate = 0;
                    } else {
                        r.seeders = reply.seeders;
                        r.leechers = reply.leechers;
                        r.status = reply.leechers == 0 ? "Nessun leecher" : "Attivo";
                        r.due = Clock::now() + std::chrono::seconds(reply.interval);
                        if (!config.keepZero && reply.leechers == 0) {
                            r.wanted = false;
                            log("Pausa per assenza leecher: " + r.meta.name);
                        }
                    }
                    log((event == "started"   ? "Avviato: "
                         : event == "stopped" ? "Arrestato: "
                                              : "Annuncio riuscito: ") +
                        r.meta.name);
                } else {
                    r.error = error;
                    r.rate = 0;
                    ++r.errors;
                    log("Errore " + r.meta.name + ": " + error);
                    if (event == "stopped") {
                        r.started = false;
                        r.status = "In pausa (stop fallito)";
                    } else {
                        r.status = "Errore / nuovo tentativo";
                        r.due = Clock::now() +
                                std::chrono::seconds(
                                    (std::min)(1800, 60 * (1 << (std::min)(r.errors - 1, 4))));
                        if (r.errors >= 5) {
                            r.wanted = false;
                            r.status = "Errore / in pausa";
                        }
                    }
                }
            }
        }
        std::lock_guard<std::mutex> lock(mu);
        try {
            saveLocked();
        } catch (const std::exception &e) {
            log(e.what());
        }
    }

    TrackerReply announceTo(const TorrentRow &task, const std::string &url,
                            const std::string &event, int port, uint64_t &bytes) {
        if (udpUrl(url)) {
            auto numwant =
                task.profile.at(event == "stopped" ? "numwantOnStop" : "numwant").get<int>();
            auto udp = udpAnnounce(url, task.meta.hash, task.peer, task.key, task.uploaded, event,
                                   port, numwant, random32(), intervalFloor);
            bytes += udp.received;
            TrackerReply reply;
            reply.interval = udp.interval;
            reply.seeders = udp.seeders;
            reply.leechers = udp.leechers;
            return reply;
        }
        auto query =
            announceQuery(task.profile, task.meta, task.peer, task.key, task.uploaded, event, port);
        auto full = url + (url.find('?') == std::string::npos ? "?" : "&") + query;
        auto result = httpGet(full, task.profile);
        bytes += result.received;
        return parseReply(result.body, intervalFloor);
    }

  public:
    DeskEngine(fs::path dir, fs::path profileDir, int floor = 30)
        : data(std::move(dir)), profiles(std::move(profileDir)), intervalFloor(floor) {
        fs::create_directories(data / L"torrents");
        if (fs::exists(data / L"state.json")) {
            try {
                auto state = readJson(data / L"state.json");
                auto c = state.at("config");
                validateConfig(c);
                configFrom(c);
                if (state.contains("keys"))
                    keyCache = state.at("keys").get<std::map<std::string, std::string>>();
                for (const auto &item : state.at("torrents")) {
                    try {
                        auto id = item.at("hash").get<std::string>();
                        if (!std::regex_match(id, std::regex("[0-9a-f]{40}")))
                            throw std::runtime_error("Infohash salvato non valido.");
                        auto path = data / L"torrents" / (wide(id) + L".torrent");
                        TorrentRow r;
                        r.meta = parseTorrent(readTorrent(path));
                        if (hex(r.meta.hash) != id)
                            throw std::runtime_error("Infohash salvato non corrispondente.");
                        r.source = path;
                        r.lifetime = item.value("lifetime", uint64_t(0));
                        rows.push_back(std::move(r));
                    } catch (const std::exception &e) {
                        log(e.what());
                    }
                }
                log("Sessione ripristinata in pausa.");
            } catch (const std::exception &e) {
                log(std::string("Sessione non caricata: ") + e.what());
                std::error_code ec;
                fs::copy_file(
                    data / L"state.json",
                    data / (L"state-recovery-" + std::to_wstring(GetTickCount64()) + L".json"),
                    fs::copy_options::none, ec);
            }
        }
        worker = std::thread([this] { tick(); });
    }
    ~DeskEngine() { shutdown(); }
    void shutdown() {
        if (worker.joinable()) {
            {
                std::lock_guard<std::mutex> lock(mu);
                quit = true;
            }
            cv.notify_all();
            worker.join();
        }
    }
    void configFrom(const json &c) {
        config.minimum = c.at("minUploadRate");
        config.maximum = c.at("maxUploadRate");
        config.simultaneous = c.at("simultaneousSeed");
        config.client = c.at("client");
        config.keepZero = c.at("keepTorrentWithZeroLeechers");
        config.target = c.value("uploadRatioTarget", -1.0);
        config.port = c.value("port", 49152);
        if (config.port < 1 || config.port > 65535)
            throw std::runtime_error("Porta non valida.");
    }
    DeskConfig getConfig() const {
        std::lock_guard<std::mutex> lock(mu);
        return config;
    }
    void setConfig(const json &c) {
        validateConfig(c);
        int port = c.value("port", 49152);
        if (port < 1 || port > 65535)
            throw std::runtime_error("Porta non valida.");
        auto p = readJson(profiles / fs::u8path(c.at("client").get<std::string>()));
        if (!supportedProfile(p))
            throw std::runtime_error("Profilo consultabile ma non ancora supportato dal motore. "
                                     "Scegliere qBittorrent, Deluge o Transmission.");
        std::lock_guard<std::mutex> lock(mu);
        for (const auto &r : rows)
            if (r.started || r.wanted || r.busy)
                throw std::runtime_error(
                    "Mettere tutti i torrent in pausa prima di cambiare impostazioni.");
        auto previous = config;
        configFrom(c);
        try {
            saveLocked();
        } catch (...) {
            config = previous;
            throw;
        }
        ++configRevision;
        log("Impostazioni salvate.");
    }
    bool add(const fs::path &path) {
        auto bytes = readTorrent(path);
        auto meta = parseTorrent(bytes);
        std::lock_guard<std::mutex> lock(mu);
        if (rows.size() >= 2000)
            throw std::runtime_error("Limite di 2000 torrent raggiunto.");
        for (const auto &r : rows)
            if (r.meta.hash == meta.hash)
                return false;
        auto dest = data / L"torrents" / (wide(hex(meta.hash)) + L".torrent");
        {
            std::ofstream out(dest, std::ios::binary | std::ios::trunc);
            if (!out.write(bytes.data(), (std::streamsize)bytes.size()) || !out.flush())
                throw std::runtime_error("Salvataggio torrent fallito.");
        }
        TorrentRow r;
        r.meta = std::move(meta);
        r.source = dest;
        log("Aggiunto: " + r.meta.name);
        rows.push_back(std::move(r));
        saveLocked();
        return true;
    }
    void command(const std::vector<std::string> &ids, int action) {
        std::lock_guard<std::mutex> lock(mu);
        for (auto &r : rows)
            if (ids.empty() || std::find(ids.begin(), ids.end(), r.meta.hash) != ids.end()) {
                if (action == 1 && !r.removing) {
                    if (!r.wanted && !r.started && !r.busy) {
                        r.profileName = config.client;
                        r.errors = 0;
                        r.error.clear();
                        r.due = {};
                        r.status = "In coda";
                        r.wanted = true;
                    }
                } else {
                    r.wanted = false;
                    r.rate = 0;
                    if (!r.started && !r.busy)
                        r.status = "In pausa";
                    if (action == 3)
                        r.removing = true;
                }
            }
        cv.notify_all();
    }
    ViewState view() const {
        std::lock_guard<std::mutex> lock(mu);
        ViewState v;
        v.rows = rows;
        v.logs.assign(logs.begin(), logs.end());
        v.received = received;
        for (const auto &r : rows) {
            v.speed += r.rate;
            v.upload += r.lifetime;
            v.confirmed += r.confirmed;
        }
        return v;
    }
};
