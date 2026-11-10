#include "telemetry/telemetry_event.hpp"
#include <ctime>

namespace telemetry {

std::string toIso8601(const std::chrono::system_clock::time_point& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    struct tm tm_buf;
#if defined(_WIN32)
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return std::string(buf);
}

nlohmann::json buildHealthPayload(const TelemetryEvent& event,
                                  const std::string& app,
                                  const std::string& appVersion,
                                  const std::string& platform,
                                  const std::string& env,
                                  const std::string& deviceId) {
    nlohmann::json payload = {
        {"schema", "app-health/1"},
        {"event", event.event},
        {"app", app},
        {"app_version", appVersion},
        {"platform", platform},
        {"env", env},
        {"device_id", deviceId},
        {"session_id", event.session_id},
        {"ts_start", event.ts_start.empty() ? toIso8601() : event.ts_start},
        {"ts_end", event.ts_end.empty() ? toIso8601() : event.ts_end},
        {"metrics", event.metrics.is_object() ? event.metrics : nlohmann::json::object()},
        {"context", event.context.is_object() ? event.context : nlohmann::json::object()},
    };
    if (!event.end_reason.empty()) {
        payload["end_reason"] = event.end_reason;
    }
    return payload;
}

}  // namespace telemetry
