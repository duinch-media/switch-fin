#pragma once

#include <string>
#include <vector>
#include <functional>

namespace data {

struct DiscoveredArr {
    enum Type { RADARR, SONARR };
    Type type;
    std::string ip;
    int port;
    std::string url;
    std::string version;
    bool authOk = false;
};

class ArrScanner {
public:
    static std::vector<std::string> getCandidateSubnets();

    static void scan(
        DiscoveredArr::Type type,
        const std::string& apiKey,
        std::function<void(const DiscoveredArr&)> onFound,
        std::function<void(bool found)> onComplete
    );

    static bool isPortOpen(const std::string& ip, int port, int timeoutMs = 250);
};

} // namespace data
