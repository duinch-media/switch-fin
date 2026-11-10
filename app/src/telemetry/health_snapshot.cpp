#include "telemetry/health_snapshot.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace telemetry {

bool writeSnapshot(const std::string& path, const std::string& json) {
    // Same temp + swap as HealthQueue: a crash never leaves a half-written snapshot.
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        out << json;
        if (!out) {
            std::remove(tmp.c_str());
            return false;
        }
    }
    std::remove(path.c_str());
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

void clearSnapshot(const std::string& path) { std::remove(path.c_str()); }

bool recoverSnapshot(const std::string& path, HealthQueue& queue) {
    std::ifstream in(path);
    if (!in) return false;
    std::stringstream buf;
    buf << in.rdbuf();
    in.close();

    auto payload = nlohmann::json::parse(buf.str(), nullptr, false);
    clearSnapshot(path);
    if (!payload.is_object() || !payload.contains("event") || payload["event"] != "playback_session") return false;

    payload["end_reason"] = "crash";
    queue.append(payload.dump());
    return true;
}

}  // namespace telemetry
