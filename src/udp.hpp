#pragma once
// UDP tracker protocol (BEP 15): connect + announce over Winsock.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <climits>
#include <cstring>
#include <string>
#pragma comment(lib, "ws2_32.lib")

struct UdpReply {
    int64_t interval = 1800;
    int seeders = -1, leechers = -1;
    uint64_t received = 0;
};

inline bool udpUrl(const std::string &s) { return s.rfind("udp://", 0) == 0; }

namespace udpdetail {
inline void put32(std::string &b, uint32_t v) {
    for (int i = 3; i >= 0; --i)
        b += (char)((v >> (8 * i)) & 0xff);
}
inline void put64(std::string &b, uint64_t v) {
    put32(b, (uint32_t)(v >> 32));
    put32(b, (uint32_t)v);
}
inline uint32_t get32(const char *p) {
    return ((uint32_t)(unsigned char)p[0] << 24) | ((uint32_t)(unsigned char)p[1] << 16) |
           ((uint32_t)(unsigned char)p[2] << 8) | (uint32_t)(unsigned char)p[3];
}
inline uint64_t get64(const char *p) { return ((uint64_t)get32(p) << 32) | get32(p + 4); }

struct Winsock {
    Winsock() {
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d))
            throw std::runtime_error("Winsock non disponibile.");
    }
    ~Winsock() { WSACleanup(); }
};
inline void ensureWinsock() { static Winsock instance; }

struct Socket {
    SOCKET s = INVALID_SOCKET;
    ~Socket() {
        if (s != INVALID_SOCKET)
            closesocket(s);
    }
};

// Sends `request` and waits for a reply whose action/transaction match.
// BEP 15 uses 15 * 2^n s retries; a desktop app needs a bounded budget, so we use
// short attempts (2 s, 4 s) and let the scheduler's back-off handle the rest.
inline std::string exchange(SOCKET s, const std::string &request, uint32_t action, uint32_t tx,
                            size_t minimum, uint64_t &received) {
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (send(s, request.data(), (int)request.size(), 0) != (int)request.size())
            throw std::runtime_error("Invio UDP fallito (Winsock " +
                                     std::to_string(WSAGetLastError()) + ").");
        DWORD timeout = 2000u << attempt;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
        while (std::chrono::steady_clock::now() < deadline) {
            char buffer[2048];
            int got = recv(s, buffer, sizeof(buffer), 0);
            if (got == SOCKET_ERROR) {
                int e = WSAGetLastError();
                if (e == WSAETIMEDOUT)
                    break;
                if (e == WSAECONNRESET) // ICMP port unreachable
                    throw std::runtime_error("Tracker UDP non raggiungibile.");
                throw std::runtime_error("Ricezione UDP fallita (Winsock " + std::to_string(e) +
                                         ").");
            }
            if (got < 8 || get32(buffer + 4) != tx)
                continue; // stale or foreign datagram
            received += (uint64_t)got;
            auto act = get32(buffer);
            if (act == 3) {
                std::string why;
                for (int i = 8; i < got && why.size() < 200; ++i)
                    why += (unsigned char)buffer[i] < 0x20 ? ' ' : buffer[i];
                throw std::runtime_error("Tracker ha rifiutato l'annuncio: " +
                                         (why.empty() ? std::string("?") : why));
            }
            if (act != action || (size_t)got < minimum)
                throw std::runtime_error("Risposta tracker UDP non valida.");
            return std::string(buffer, (size_t)got);
        }
    }
    throw std::runtime_error("Tracker UDP: nessuna risposta.");
}
} // namespace udpdetail

// event: "" (periodic), "started", "stopped". key: hex string from the profile.
inline UdpReply udpAnnounce(const std::string &url, const std::string &infohash,
                            const std::string &peer, const std::string &key, uint64_t uploaded,
                            const std::string &event, int port, int numwant, uint32_t random,
                            int floor = 30) {
    using namespace udpdetail;
    if (infohash.size() != 20 || peer.size() != 20)
        throw std::runtime_error("Annuncio UDP: infohash o peer_id non validi.");
    // udp://host:port[/path] (IPv6 hosts in brackets)
    auto rest = url.substr(6);
    rest = rest.substr(0, rest.find_first_of("/?#"));
    std::string host, service;
    if (!rest.empty() && rest[0] == '[') {
        auto close = rest.find(']');
        if (close == std::string::npos || close + 1 >= rest.size() || rest[close + 1] != ':')
            throw std::runtime_error("URL tracker UDP non valido.");
        host = rest.substr(1, close - 1);
        service = rest.substr(close + 2);
    } else {
        auto colon = rest.rfind(':');
        if (colon == std::string::npos)
            throw std::runtime_error("URL tracker UDP senza porta.");
        host = rest.substr(0, colon);
        service = rest.substr(colon + 1);
    }
    if (host.empty() || service.empty() ||
        service.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("URL tracker UDP non valido.");

    ensureWinsock();
    addrinfo hints = {}, *found = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    if (getaddrinfo(host.c_str(), service.c_str(), &hints, &found) || !found)
        throw std::runtime_error("Host tracker UDP non risolto.");
    Socket sock;
    for (auto a = found; a; a = a->ai_next) {
        sock.s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (sock.s == INVALID_SOCKET)
            continue;
        if (connect(sock.s, a->ai_addr, (int)a->ai_addrlen) == 0)
            break;
        closesocket(sock.s);
        sock.s = INVALID_SOCKET;
    }
    freeaddrinfo(found);
    if (sock.s == INVALID_SOCKET)
        throw std::runtime_error("Socket UDP non disponibile.");

    UdpReply reply;
    uint32_t tx = random;
    std::string connectReq;
    put64(connectReq, 0x41727101980ull);
    put32(connectReq, 0);
    put32(connectReq, tx);
    auto connected = exchange(sock.s, connectReq, 0, tx, 16, reply.received);
    auto connection = get64(connected.data() + 8);

    uint32_t keyValue = 0;
    for (char c : key.size() > 8 ? key.substr(key.size() - 8) : key) {
        int d = isdigit((unsigned char)c) ? c - '0' : (tolower((unsigned char)c) - 'a' + 10);
        if (d < 0 || d > 15)
            throw std::runtime_error("Chiave profilo non esadecimale.");
        keyValue = keyValue * 16 + (uint32_t)d;
    }
    tx = tx * 1103515245u + 12345u;
    std::string req;
    put64(req, connection);
    put32(req, 1);
    put32(req, tx);
    req += infohash;
    req += peer;
    put64(req, 0);        // downloaded
    put64(req, 0);        // left: announced as complete
    put64(req, uploaded); // uploaded
    put32(req, event == "started" ? 2 : event == "stopped" ? 3 : 0);
    put32(req, 0); // IP: let the tracker use the source address
    put32(req, keyValue);
    put32(req, (uint32_t)(numwant < 0 ? -1 : numwant));
    req += (char)((port >> 8) & 0xff);
    req += (char)(port & 0xff);
    auto announced = exchange(sock.s, req, 1, tx, 20, reply.received);
    auto interval = (int64_t)get32(announced.data() + 8);
    if (interval <= 0 || interval > 7 * 86400)
        throw std::runtime_error("Intervallo tracker non valido.");
    reply.interval = (std::max)(interval, (int64_t)floor);
    reply.leechers = (int)(std::min)(get32(announced.data() + 12), (uint32_t)INT_MAX);
    reply.seeders = (int)(std::min)(get32(announced.data() + 16), (uint32_t)INT_MAX);
    return reply;
}
