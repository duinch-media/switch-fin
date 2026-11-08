#include "data/arr_scanner.hpp"
#include "api/http.hpp"
#include "utils/config.hpp"
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define close closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#endif

namespace data {

bool ArrScanner::isPortOpen(const std::string& ip, int port, int timeoutMs) {
#ifdef _WIN32
    static bool wsaInit = false;
    if (!wsaInit) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        wsaInit = true;
    }
#endif

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));

#ifdef _WIN32
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    struct timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    int res = select(0, nullptr, &fdset, nullptr, &tv);
    if (res > 0) {
        int so_error = 0;
        int len = sizeof(so_error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &len);
        close(sock);
        return so_error == 0;
    }
#else
    struct pollfd pfd{};
    pfd.fd = sock;
    pfd.events = POLLOUT;
    int res = poll(&pfd, 1, timeoutMs);
    if (res > 0 && (pfd.revents & POLLOUT) && !(pfd.revents & (POLLERR | POLLHUP))) {
        int so_error = 0;
        socklen_t len = sizeof(so_error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
        close(sock);
        return so_error == 0;
    }
#endif

    close(sock);
    return false;
}

std::vector<std::string> ArrScanner::getCandidateSubnets() {
    std::vector<std::string> subnets;

    // 1. From Jellyfin server URL
    std::string jfUrl = AppConfig::instance().getUrl();
    if (!jfUrl.empty()) {
        auto pos = jfUrl.find("://");
        std::string host = (pos != std::string::npos) ? jfUrl.substr(pos + 3) : jfUrl;
        auto colon = host.find(':');
        if (colon != std::string::npos) host = host.substr(0, colon);
        auto slash = host.find('/');
        if (slash != std::string::npos) host = host.substr(0, slash);

        auto lastDot = host.rfind('.');
        if (lastDot != std::string::npos && lastDot > 0) {
            std::string sub = host.substr(0, lastDot + 1);
            subnets.push_back(sub);
        }
    }

    // 2. Discover local interface IP by dummy UDP connect
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s >= 0) {
        struct sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_port = htons(53);
        inet_pton(AF_INET, "8.8.8.8", &target.sin_addr);
        if (connect(s, reinterpret_cast<struct sockaddr*>(&target), sizeof(target)) == 0) {
            struct sockaddr_in local{};
            socklen_t len = sizeof(local);
            if (getsockname(s, reinterpret_cast<struct sockaddr*>(&local), &len) == 0) {
                char ipBuf[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &local.sin_addr, ipBuf, sizeof(ipBuf));
                std::string localIp(ipBuf);
                auto lastDot = localIp.rfind('.');
                if (lastDot != std::string::npos && lastDot > 0) {
                    std::string sub = localIp.substr(0, lastDot + 1);
                    if (std::find(subnets.begin(), subnets.end(), sub) == subnets.end()) {
                        subnets.push_back(sub);
                    }
                }
            }
        }
        close(s);
    }

    // 3. Fallbacks
    std::vector<std::string> fallbacks = {"192.168.1.", "192.168.0.", "10.0.0."};
    for (const auto& fb : fallbacks) {
        if (std::find(subnets.begin(), subnets.end(), fb) == subnets.end()) {
            subnets.push_back(fb);
        }
    }

    return subnets;
}

void ArrScanner::scan(
    DiscoveredArr::Type type,
    const std::string& apiKey,
    std::function<void(const DiscoveredArr&)> onFound,
    std::function<void(bool found)> onComplete)
{
    brls::async([type, apiKey, onFound, onComplete]() {
        const int port = (type == DiscoveredArr::RADARR) ? 7878 : 8989;
        const std::string appName = (type == DiscoveredArr::RADARR) ? "Radarr" : "Sonarr";
        std::atomic<bool> foundAny{false};

        // Quick check Jellyfin host first
        std::string jfUrl = AppConfig::instance().getUrl();
        std::string jfHost = "";
        if (!jfUrl.empty()) {
            auto pos = jfUrl.find("://");
            jfHost = (pos != std::string::npos) ? jfUrl.substr(pos + 3) : jfUrl;
            auto colon = jfHost.find(':');
            if (colon != std::string::npos) jfHost = jfHost.substr(0, colon);
            auto slash = jfHost.find('/');
            if (slash != std::string::npos) jfHost = jfHost.substr(0, slash);
        }

        auto testIp = [&](const std::string& ip) -> bool {
            if (!isPortOpen(ip, port, 300)) return false;

            std::string url = "http://" + ip + ":" + std::to_string(port);
            std::string statusUrl = url + "/api/v3/system/status";
            if (!apiKey.empty()) {
                statusUrl += "?apikey=" + apiKey;
            }

            HTTP::Header headers = {"Accept: application/json"};
            if (!apiKey.empty()) {
                headers.push_back("X-Api-Key: " + apiKey);
            }

            try {
                std::string resp = HTTP::get(statusUrl, headers, HTTP::Timeout{1500});
                nlohmann::json j = nlohmann::json::parse(resp);
                DiscoveredArr d;
                d.type = type;
                d.ip = ip;
                d.port = port;
                d.url = url;
                d.version = j.value("version", "");
                d.authOk = true;

                foundAny.store(true);
                brls::sync([onFound, d]() { onFound(d); });
                return true;
            } catch (...) {
                // If 401 Unauthorized or other response on target port, server still exists
                DiscoveredArr d;
                d.type = type;
                d.ip = ip;
                d.port = port;
                d.url = url;
                d.version = "Auth Required";
                d.authOk = false;

                foundAny.store(true);
                brls::sync([onFound, d]() { onFound(d); });
                return true;
            }
        };

        if (!jfHost.empty() && testIp(jfHost)) {
            brls::sync([onComplete]() { onComplete(true); });
            return;
        }

        auto subnets = getCandidateSubnets();
        std::string primarySubnet = subnets.empty() ? "192.168.1." : subnets.front();

        // Scan primary subnet in concurrent batches of 16 threads
        const int batchSize = 16;
        for (int i = 1; i <= 254 && !foundAny.load(); i += batchSize) {
            std::vector<std::thread> workers;
            for (int j = i; j < i + batchSize && j <= 254 && !foundAny.load(); ++j) {
                std::string ip = primarySubnet + std::to_string(j);
                if (ip == jfHost) continue;
                workers.emplace_back([&testIp, ip, &foundAny]() {
                    if (!foundAny.load()) {
                        testIp(ip);
                    }
                });
            }
            for (auto& w : workers) {
                if (w.joinable()) w.join();
            }
        }

        brls::sync([onComplete, &foundAny]() {
            onComplete(foundAny.load());
        });
    });
}

} // namespace data
