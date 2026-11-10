#include "telemetry/telemetry_client.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

#include <borealis/core/logger.hpp>

#include "api/http.hpp"
#include "telemetry/health_uploader.hpp"
#include "utils/config.hpp"
#include "utils/misc.hpp"
#include "utils/thread.hpp"

namespace telemetry {

namespace {

// Gateway label rule: ^[a-z0-9][a-z0-9_-]{1,31}$ (platform is a Loki label).
std::string platformName() {
#ifdef __SWITCH__
    return "switch";
#else
    std::string p = AppVersion::getPlatform();
    for (auto& c : p) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const std::regex ok("^[a-z0-9][a-z0-9_-]{1,31}$");
    return std::regex_match(p, ok) ? p : "unknown";
#endif
}

// Parse "http status 503" thrown by HTTP::post; 0 means a network-level failure.
int statusFromError(const std::string& what) {
    static const std::regex re("http status (\\d+)");
    std::smatch m;
    return std::regex_search(what, m, re) ? std::stoi(m[1].str()) : 0;
}

}  // namespace

TelemetryClient& TelemetryClient::instance() {
    static TelemetryClient s_instance;
    return s_instance;
}

void TelemetryClient::init(const std::string& configDir) {
    if (this->queue) return;
    this->config = loadHealthConfig(healthConfigPaths(configDir));
    this->deviceId = loadOrCreateDeviceId(configDir + "/health_device_id");
    this->queue = std::make_unique<HealthQueue>(configDir + "/health_pending.ndjson");
    brls::Logger::info("Telemetry: {} ({} queued from earlier runs)",
        this->config.enabled ? "reporting to " + this->config.endpoint : "no health.json, recording locally only",
        this->queue->size());
    if (this->config.enabled && this->queue->size() > 0) this->flushAsync();
}

nlohmann::json TelemetryClient::buildPayload(const PlaybackEvent& event) const {
    nlohmann::json metrics = nlohmann::json::object();
    const auto& m = event.metrics;
    // Omit what was not measured: a missing key means "unknown", 0 means "really zero".
    if (m.init_ms > 0) metrics["init_ms"] = m.init_ms;
    if (m.api_ms > 0) metrics["api_ms"] = m.api_ms;
    if (m.open_ms > 0) metrics["open_ms"] = m.open_ms;
    if (m.ttff_ms > 0) metrics["ttff_ms"] = m.ttff_ms;
    metrics["play_ms"] = m.play_ms;
    metrics["stall_count"] = m.stall_count;
    metrics["stall_ms"] = m.stall_ms;
    metrics["rebuffer_ratio"] = m.rebuffer_ratio;
    metrics["seek_count"] = m.seek_count;
    if (m.seek_count > 0 && m.seek_p50_ms > 0) metrics["seek_p50_ms"] = m.seek_p50_ms;
    if (m.input_kbps_avg > 0) metrics["input_kbps_avg"] = m.input_kbps_avg;
    if (m.input_kbps_p10 > 0) metrics["input_kbps_p10"] = m.input_kbps_p10;
    metrics["dropped_frames"] = m.dropped_frames;

    nlohmann::json context = nlohmann::json::object();
    const auto& c = event.context;
    if (!c.play_method.empty()) context["play_method"] = c.play_method;
    if (!c.video_codec.empty()) context["video_codec"] = c.video_codec;
    if (!c.hwdec.empty()) context["hwdec"] = c.hwdec;
    if (!c.resolution.empty()) context["resolution"] = c.resolution;
    if (c.bitrate_kbps > 0) context["bitrate_kbps"] = c.bitrate_kbps;
    if (c.wifi_strength >= 0) context["wifi_strength"] = c.wifi_strength;
    context["docked"] = c.docked;

    return {
        {"schema", "app-health/1"},
        {"event", "playback_session"},
        {"app", "switchfin"},
        {"app_version", AppVersion::getVersion()},
        {"platform", platformName()},
        {"env", this->config.env},
        {"device_id", this->deviceId},
        {"session_id", event.session_id.empty() ? misc::randHex(16) : event.session_id},
        {"ts_start", event.ts_start},
        {"ts_end", event.ts_end},
        {"end_reason", event.end_reason},
        {"metrics", metrics},
        {"context", context},
    };
}

void TelemetryClient::recordPlayback(const PlaybackEvent& event) {
    if (!this->queue) return;  // init() not called (e.g. very early exit): drop silently
    try {
        this->queue->append(this->buildPayload(event).dump());
        brls::Logger::info("Telemetry: queued playback session {}", event.session_id);
    } catch (const std::exception& e) {
        brls::Logger::warning("Telemetry: cannot queue event: {}", e.what());
    }
}

void TelemetryClient::flushAsync() {
    if (!this->queue || !this->config.enabled) return;
    ThreadPool::instance().submit([this](HTTP&) { this->drain(5000); });
}

void TelemetryClient::shutdown() {
    if (!this->queue || !this->config.enabled) return;
    this->drain(2000);
}

void TelemetryClient::drain(long timeoutMs) {
    if (this->busy.exchange(true)) return;  // another drain is already running
    const std::string url = this->config.endpoint;
    HTTP::Header headers = {
        "Content-Type: application/x-ndjson",
        "Authorization: Bearer " + this->config.apiKey,  // never logged
    };
    auto transport = [&](const std::string& body) -> int {
        try {
            HTTP::post(url, body, headers, HTTP::Timeout{timeoutMs});
            return 200;
        } catch (const std::exception& e) {
            return statusFromError(e.what());
        }
    };
    auto res = uploadPending(*this->queue, transport);
    if (res.sent || res.discarded || !res.completed) {
        brls::Logger::info("Telemetry: sent {}, discarded {}, {}", res.sent, res.discarded,
            res.completed ? "queue empty" : "kept the rest for the next trigger");
    }
    this->busy = false;
}

}  // namespace telemetry
