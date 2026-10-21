#pragma once
#include <windows.h>
#include <string>
inline std::wstring wide(const std::string &s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    if (n)
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    for (auto &c : w)
        if (c == 0 || c == L'\r' || c == L'\n')
            c = L' ';
    return w;
}
inline std::string utf8(const std::wstring &s) {
    if (s.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string t(n, 0);
    if (n)
        WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), t.data(), n, nullptr, nullptr);
    return t;
}
inline std::wstring sizeText(uint64_t bytes) {
    const wchar_t *units[] = {L"B", L"KiB", L"MiB", L"GiB", L"TiB", L"PiB"};
    double v = (double)bytes;
    int u = 0;
    while (v >= 1024 && u < 5) {
        v /= 1024;
        ++u;
    }
    wchar_t buf[80];
    swprintf_s(buf, L"%.2f %s", v, units[u]);
    return buf;
}
