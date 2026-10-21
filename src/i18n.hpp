#pragma once
#include "core.hpp"
#include "text.hpp"
#include <map>
#include <regex>
class I18n {
    json english = json::object(), selected = json::object();
    std::string language = "en";
    static std::wstring decode(const std::string &s) {
        if (s.empty())
            return {};
        int n =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0);
        if (!n)
            throw std::runtime_error("Invalid UTF-8 translation.");
        std::wstring out(n, 0);
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
        return out;
    }
    static json resourceEnglish() {
        auto resource = FindResource(nullptr, MAKEINTRESOURCE(301), RT_RCDATA);
        if (!resource)
            return json::object();
        auto data = LoadResource(nullptr, resource);
        auto bytes = static_cast<const char *>(LockResource(data));
        if (!bytes)
            return json::object();
        return json::parse(bytes, bytes + SizeofResource(nullptr, resource)).at("strings");
    }

  public:
    void load(const fs::path &root, const std::string &code) {
        if (!std::regex_match(code, std::regex("[a-z]{2}(-[A-Z]{2})?")))
            throw std::runtime_error("Invalid language code.");
        english = resourceEnglish();
        try {
            if (fs::exists(root / "languages" / "en.json")) {
                auto external = readJson(root / "languages" / "en.json").at("strings");
                if (external.is_object()) {
                    for (const auto &entry : external.items()) {
                        if (!entry.value().is_string())
                            throw std::runtime_error("Invalid translation value.");
                        decode(entry.value().get<std::string>());
                    }
                    english = external;
                }
            }
        } catch (...) {
        }
        selected = code == "en" ? english : json::object();
        auto path = root / "languages" / (code + ".json");
        if (code != "en" && fs::exists(path))
            selected = readJson(path).at("strings");
        if (!selected.is_object())
            throw std::runtime_error("Invalid translation catalog.");
        for (const auto &v : selected.items())
            if (!v.value().is_string())
                throw std::runtime_error("Invalid translation value.");
        language = code;
    }
    const std::string &code() const { return language; }
    std::wstring get(const std::wstring &original) const {
        auto key = utf8(original);
        if (selected.contains(key))
            return decode(selected.at(key).get<std::string>());
        if (language == "it")
            return original;
        if (english.contains(key))
            return decode(english.at(key).get<std::string>());
        return original;
    }
    std::wstring message(const std::wstring &original) const {
        auto exact = get(original);
        if (exact != original || language == "it")
            return exact;
        // Only translate known message prefixes; never rewrite torrent names or paths.
        size_t longest = 0;
        std::wstring translated;
        for (const auto &kv : english.items()) {
            auto key = wide(kv.key());
            if (key.size() > longest && key.size() >= 4 &&
                (key.back() == L' ' || key.back() == L':') && original.rfind(key, 0) == 0) {
                longest = key.size();
                translated = get(key);
            }
        }
        return longest ? translated + original.substr(longest) : original;
    }
};
