#pragma once

#include <string>
#include <vector>

namespace telemetry {

/// Local, uncommitted settings for the app-health gateway. There are no
/// defaults in code: with no endpoint or key the app only queues events.
struct HealthConfig {
    bool enabled = false;  // true only when endpoint and apiKey are present
    std::string endpoint;  // e.g. http://192.168.1.37:8089/v1/events
    std::string apiKey;
    std::string env = "prod";  // prod|beta|dev|test
};

/// Where health.json is looked up (same places as arr_config.json).
std::vector<std::string> healthConfigPaths(const std::string& configDir);

/// First readable, valid file wins. Missing or invalid files yield a disabled config.
HealthConfig loadHealthConfig(const std::vector<std::string>& paths);

/// Random install UUID (v4), created once and stored at path. Never a hardware id.
std::string loadOrCreateDeviceId(const std::string& path);

}  // namespace telemetry
