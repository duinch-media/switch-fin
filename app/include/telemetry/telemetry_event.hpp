#pragma once

#include <chrono>
#include <string>
#include <nlohmann/json.hpp>

namespace telemetry {

struct TelemetryEvent {
    std::string event;
    std::string session_id;
    std::string ts_start;
    std::string ts_end;
    std::string end_reason;
    nlohmann::json metrics = nlohmann::json::object();
    nlohmann::json context = nlohmann::json::object();
};

/// Format a system_clock time point as RFC3339 / ISO8601 UTC string ("YYYY-MM-DDTHH:MM:SSZ").
std::string toIso8601(const std::chrono::system_clock::time_point& tp = std::chrono::system_clock::now());

/// Assemble the final app-health/1 JSON envelope from a TelemetryEvent.
nlohmann::json buildHealthPayload(const TelemetryEvent& event,
                                  const std::string& app,
                                  const std::string& appVersion,
                                  const std::string& platform,
                                  const std::string& env,
                                  const std::string& deviceId);

}  // namespace telemetry
