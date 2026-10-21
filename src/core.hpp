#pragma once
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "../vendor/json.hpp"
namespace fs = std::filesystem;
using nlohmann::json;
inline json readJson(const fs::path &path) {
    if (fs::file_size(path) > 4 * 1024 * 1024)
        throw std::runtime_error("File JSON troppo grande (massimo 4 MB).");
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("Impossibile aprire il file.");
    return json::parse(in);
}
inline void validateConfig(const json &j) {
    for (auto k : {"minUploadRate", "maxUploadRate", "simultaneousSeed"})
        if (!j.contains(k) || !j[k].is_number_integer() || j[k].get<double>() < 0 ||
            j[k].get<double>() > 2147483647)
            throw std::runtime_error("Valore intero mancante o fuori intervallo.");
    if (j["minUploadRate"].get<int>() > j["maxUploadRate"].get<int>() ||
        j["simultaneousSeed"].get<int>() < 1)
        throw std::runtime_error("Intervallo velocita o numero sessioni non valido.");
    if (!j.contains("client") || !j["client"].is_string())
        throw std::runtime_error("Profilo client mancante.");
    auto name = j["client"].get<std::string>();
    if (name.empty() || name.find_first_of("/\\:") != std::string::npos || name == "..")
        throw std::runtime_error("Nome profilo non valido.");
    if (!j.contains("keepTorrentWithZeroLeechers") ||
        !j["keepTorrentWithZeroLeechers"].is_boolean())
        throw std::runtime_error("keepTorrentWithZeroLeechers deve essere booleano.");
    if (j.contains("uploadRatioTarget") &&
        (!j["uploadRatioTarget"].is_number() ||
         (j["uploadRatioTarget"].get<double>() < 0 && j["uploadRatioTarget"].get<double>() != -1)))
        throw std::runtime_error("Ratio non valido.");
}
