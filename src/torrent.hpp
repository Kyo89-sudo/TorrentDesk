#pragma once
#include "core.hpp"
#include <winsock2.h> // must precede windows.h (UDP trackers)
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <map>
#include <vector>
#include <charconv>
#include <limits>
struct BValue {
    enum Kind { Integer, String, List, Dict } kind = String;
    int64_t number = 0;
    std::string text;
    std::vector<BValue> list;
    std::map<std::string, BValue> dict;
    size_t begin = 0, end = 0;
    const BValue &at(const std::string &key) const {
        if (kind != Dict || !dict.count(key))
            throw std::runtime_error("Metadati mancanti: " + key);
        return dict.at(key);
    }
    bool has(const std::string &key) const { return kind == Dict && dict.count(key); }
};
class BParser {
    const std::string &s;
    size_t p = 0, nodes = 0;
    int64_t number(const std::string &t) {
        int64_t v;
        auto r = std::from_chars(t.data(), t.data() + t.size(), v);
        if (r.ec != std::errc() || r.ptr != t.data() + t.size() || t.empty() || t == "-0" ||
            (t.size() > 1 && t[0] == '0') || (t.size() > 2 && t[0] == '-' && t[1] == '0'))
            throw std::runtime_error("Intero bencode non valido.");
        return v;
    }
    BValue value(int depth) {
        if (depth > 64 || ++nodes > 250000 || p >= s.size())
            throw std::runtime_error("Bencode troncato o troppo complesso.");
        BValue v;
        v.begin = p;
        char c = s[p++];
        if (c == 'i') {
            auto e = s.find('e', p);
            if (e == std::string::npos)
                throw std::runtime_error("Intero bencode troncato.");
            v.kind = BValue::Integer;
            v.number = number(s.substr(p, e - p));
            p = e + 1;
        } else if (c == 'l' || c == 'd') {
            v.kind = c == 'l' ? BValue::List : BValue::Dict;
            while (p < s.size() && s[p] != 'e') {
                if (c == 'l')
                    v.list.push_back(value(depth + 1));
                else {
                    auto k = value(depth + 1);
                    if (k.kind != BValue::String || v.dict.count(k.text))
                        throw std::runtime_error("Chiave bencode duplicata o non valida.");
                    v.dict.emplace(k.text, value(depth + 1));
                }
            }
            if (p >= s.size())
                throw std::runtime_error("Bencode troncato.");
            ++p;
        } else if (c >= '0' && c <= '9') {
            --p;
            auto e = s.find(':', p);
            if (e == std::string::npos || e - p > 9)
                throw std::runtime_error("Lunghezza bencode non valida.");
            auto len = number(s.substr(p, e - p));
            if (len < 0 || (uint64_t)len > s.size() - e - 1)
                throw std::runtime_error("Stringa bencode troncata.");
            p = e + 1;
            v.kind = BValue::String;
            v.text = s.substr(p, (size_t)len);
            p += (size_t)len;
        } else
            throw std::runtime_error("Token bencode non valido.");
        v.end = p;
        return v;
    }

  public:
    explicit BParser(const std::string &data) : s(data) {}
    BValue parse() {
        auto v = value(0);
        if (p != s.size())
            throw std::runtime_error("Dati extra dopo il bencode.");
        return v;
    }
};
inline std::string sha1(const std::string &data) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    unsigned char digest[20];
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("SHA-1 non disponibile.");
    auto status = BCryptHash(alg, nullptr, 0, (PUCHAR)data.data(), (ULONG)data.size(), digest, 20);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (status < 0)
        throw std::runtime_error("Calcolo infohash fallito.");
    return std::string((char *)digest, 20);
}
inline std::string hex(const std::string &data) {
    std::string s;
    for (unsigned char c : data) {
        s += "0123456789abcdef"[c >> 4];
        s += "0123456789abcdef"[c & 15];
    }
    return s;
}
struct TorrentMeta {
    std::string name, hash;
    uint64_t size = 0;
    int files = 1;
    std::vector<std::string> trackers;
};
inline TorrentMeta parseTorrent(const std::string &bytes) {
    auto root = BParser(bytes).parse();
    const auto &info = root.at("info");
    if (info.kind != BValue::Dict)
        throw std::runtime_error("Info torrent non valido.");
    if (!info.has("pieces"))
        throw std::runtime_error("Torrent v2 puro non supportato; usare v1 o ibrido.");
    if (info.at("pieces").kind != BValue::String || info.at("pieces").text.size() % 20)
        throw std::runtime_error("Hashes dei pezzi non validi.");
    if (info.at("piece length").kind != BValue::Integer || info.at("piece length").number <= 0)
        throw std::runtime_error("Dimensione pezzo non valida.");
    TorrentMeta t;
    const auto &name = info.has("name.utf-8") ? info.at("name.utf-8") : info.at("name");
    if (name.kind != BValue::String || name.text.empty())
        throw std::runtime_error("Nome torrent non valido.");
    t.name = name.text;
    auto addLength = [&](const BValue &v) {
        if (v.kind != BValue::Integer || v.number < 0 || (uint64_t)v.number > INT64_MAX - t.size)
            throw std::runtime_error("Dimensione torrent non valida.");
        t.size += (uint64_t)v.number;
    };
    if (info.has("files")) {
        const auto &files = info.at("files");
        if (files.kind != BValue::List || files.list.empty())
            throw std::runtime_error("Elenco file non valido.");
        t.files = (int)files.list.size();
        for (const auto &file : files.list)
            addLength(file.at("length"));
    } else
        addLength(info.at("length"));
    uint64_t pieceLen = (uint64_t)info.at("piece length").number;
    if (info.at("pieces").text.size() / 20 != t.size / pieceLen + (t.size % pieceLen ? 1 : 0))
        throw std::runtime_error("Numero di pezzi incoerente con la dimensione.");
    t.hash = sha1(bytes.substr(info.begin, info.end - info.begin));
    auto tracker = [&](const BValue &v) {
        if (v.kind != BValue::String || v.text.size() > 8192)
            throw std::runtime_error("URL tracker non valido.");
        if (std::find(t.trackers.begin(), t.trackers.end(), v.text) == t.trackers.end())
            t.trackers.push_back(v.text);
    };
    if (root.has("announce"))
        tracker(root.at("announce"));
    if (root.has("announce-list")) {
        const auto &tiers = root.at("announce-list");
        if (tiers.kind != BValue::List)
            throw std::runtime_error("announce-list non valida.");
        for (const auto &tier : tiers.list) {
            if (tier.kind != BValue::List)
                throw std::runtime_error("Tier tracker non valido.");
            for (const auto &v : tier.list)
                tracker(v);
        }
    }
    return t;
}
inline std::string readTorrent(const fs::path &path) {
    if (fs::file_size(path) > 32 * 1024 * 1024)
        throw std::runtime_error("Torrent troppo grande (massimo 32 MB).");
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Impossibile leggere il torrent.");
    return std::string(std::istreambuf_iterator<char>(in), {});
}
