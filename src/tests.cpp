#include "engine.hpp"
#include "i18n.hpp"
#include <iostream>
#define REQUIRE(x)                                                                                 \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error("FAIL " #x);                                                  \
    } while (0)
std::string str(const std::string &s) { return std::to_string(s.size()) + ":" + s; }
std::string testTorrent(const std::string &url = "http://127.0.0.1:18765/announce",
                        bool multi = false) {
    std::string info = "d";
    if (multi)
        info += "5:filesld6:lengthi3e4:pathl1:aeed6:lengthi2e4:pathl1:beee";
    else
        info += "6:lengthi5e";
    info += "4:name8:test.bin12:piece lengthi16384e6:pieces20:" + std::string(20, 'x') + "e";
    return "d8:announce" + str(url) + "4:info" + info + "e";
}
void rejects(const std::function<void()> &f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    REQUIRE(rejected);
}
int main(int argc, char **argv) {
    try {
        REQUIRE(argc >= 2);
        fs::path base = fs::u8path(argv[1]);
        auto english = readJson(base / "languages" / "en.json").at("strings");
        auto italian = readJson(base / "languages" / "it.json").at("strings");
        REQUIRE(english.size() == italian.size());
        for (const auto &item : english.items()) {
            REQUIRE(italian.contains(item.key()));
            REQUIRE(item.value().is_string());
            REQUIRE(italian.at(item.key()).is_string());
        }
        I18n translations;
        translations.load(base, "en");
        REQUIRE(translations.get(L"Avvia") == L"Start");
        REQUIRE(translations.get(L"Aiuto") == L"Help");
        REQUIRE(translations.message(L"Avviato: untouched-name") == L"Started: untouched-name");
        REQUIRE(translations.message(L"In pausa") == L"Paused");
        REQUIRE(translations.get(L"\r\nDimensione: ") == L"\r\nSize: ");
        translations.load(base, "it");
        REQUIRE(translations.get(L"Avvia") == L"Avvia");
        rejects([&] { translations.load(base, "../en"); });
        std::cout << "PASS: EN/IT catalogs, language switching, multiline text, dynamic messages "
                     "and path validation\n";
        int count = 0, supported = 0;
        for (auto &e : fs::directory_iterator(base / "profiles")) {
            auto p = readJson(e.path());
            REQUIRE(p.contains("peerIdGenerator") && p.contains("query") &&
                    p.contains("requestHeaders"));
            if (supportedProfile(p)) {
                for (int i = 0; i < 10; ++i) {
                    REQUIRE(peerId(p).size() == 20);
                    REQUIRE(!makeKey(p).empty());
                }
                ++supported;
            }
            ++count;
        }
        auto good = json::parse(
            R"({"minUploadRate":30,"maxUploadRate":170,"simultaneousSeed":20,"client":"qbittorrent-5.2.4.client","keepTorrentWithZeroLeechers":true})");
        validateConfig(good);
        for (auto bad : {json("30"), json(-1), json(2147483648ULL)}) {
            auto j = good;
            j["minUploadRate"] = bad;
            rejects([&] { validateConfig(j); });
        }
        for (auto name : {"../test.client", "C:\\test.client", ""}) {
            auto j = good;
            j["client"] = name;
            rejects([&] { validateConfig(j); });
        }
        auto invalid = good;
        invalid["maxUploadRate"] = 1;
        rejects([&] { validateConfig(invalid); });
        for (auto s : {"", "i-0e", "i01e", "3:ab", "d1:ai1e1:ai2ee", "li1e",
                       "i9223372036854775808e", "i2ee"})
            rejects([&] { BParser(s).parse(); });
        std::string deep(66, 'l');
        deep += std::string(66, 'e');
        rejects([&] { BParser(deep).parse(); });
        auto fixture = testTorrent();
        auto meta = parseTorrent(fixture);
        REQUIRE(meta.name == "test.bin" && meta.size == 5 && meta.files == 1);
        REQUIRE(hex(meta.hash) == "88e2a52f41b5a1c86cb34927213e216a579a9ba8");
        auto multi = parseTorrent(testTorrent("http://localhost/announce", true));
        REQUIRE(multi.size == 5 && multi.files == 2);
        auto profile = readJson(base / "profiles" / "qbittorrent-5.2.4.client");
        auto peer = peerId(profile), key = makeKey(profile);
        auto query = announceQuery(profile, meta, peer, key, 123, "started", 49152);
        REQUIRE(query.find("uploaded=123") != std::string::npos &&
                query.find("event=started") != std::string::npos &&
                query.find("left=0") != std::string::npos);
        REQUIRE(announceQuery(profile, meta, peer, key, 123, "", 49152).find("event=") ==
                std::string::npos);
        REQUIRE(announceQuery(profile, meta, peer, key, 123, "stopped", 49152).find("numwant=0") !=
                std::string::npos);
        REQUIRE(encode(std::string("\0\xff", 2), profile) == "%00%ff");
        REQUIRE(parseReply("d8:completei2e10:incompletei3e8:intervali1e12:min intervali45ee")
                    .interval == 45);
        rejects([&] { parseReply("d14:failure reason4:nopee"); });
        rejects([&] { parseReply("d8:intervali-1ee"); });
        auto work = fs::current_path() / L"work" / L"TorrentDeskTests" /
                    std::to_wstring(GetCurrentProcessId());
        fs::create_directories(work);
        auto torrent = work / L"unicodé-测试.torrent";
        {
            std::ofstream out(torrent, std::ios::binary);
            out << fixture;
        }
        {
            DeskEngine engine(work / "session", base / "profiles");
            REQUIRE(engine.add(torrent));
            REQUIRE(!engine.add(torrent));
            auto c = good;
            c["port"] = 0;
            rejects([&] { engine.setConfig(c); });
            engine.setConfig(good);
            REQUIRE(engine.view().rows.size() == 1);
            engine.shutdown();
        }
        {
            DeskEngine engine(work / "session", base / "profiles");
            REQUIRE(engine.view().rows.size() == 1);
            REQUIRE(!engine.view().rows[0].wanted);
            engine.shutdown();
        }
        std::cout << "PASS: " << count << " profili JSON (" << supported
                  << " eseguibili), peer_id, chiavi, bencode, infohash, multifile, validazione, "
                     "URL, risposte e persistenza Unicode\n";
        if (argc > 2 && std::string(argv[2]) == "--network") {
            auto result = httpGet("http://127.0.0.1:18765/health", profile);
            REQUIRE(result.body == "ok");
            auto reply = httpGet("http://127.0.0.1:18765/announce?" + query, profile);
            REQUIRE(parseReply(reply.body, 1).leechers == 3);
            rejects([&] { httpGet("http://127.0.0.1:18765/redirect", profile); });
            rejects([&] { httpGet("http://127.0.0.1:18765/fail", profile); });
            {
                DeskEngine engine(work / "network-session", base / "profiles", 1);
                engine.add(torrent);
                engine.setConfig(good);
                engine.command({}, 1);
                bool active = false;
                for (int i = 0; i < 70; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    auto v = engine.view();
                    if (v.rows[0].started && v.rows[0].confirmed > 0) {
                        active = true;
                        break;
                    }
                }
                REQUIRE(active);
                auto v = engine.view();
                REQUIRE(v.rows[0].uploaded > 0 && v.rows[0].leechers == 3);
                engine.command({}, 2);
                bool paused = false;
                for (int i = 0; i < 70; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    if (!engine.view().rows[0].started) {
                        paused = true;
                        break;
                    }
                }
                REQUIRE(paused);
                auto before = engine.view().rows[0].lifetime;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                REQUIRE(engine.view().rows[0].lifetime == before);
                engine.shutdown();
            }
            std::cout << "PASS: HTTP locale, errori/redirect, scheduler started/periodico/stopped, "
                         "upload e pausa\n";
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
