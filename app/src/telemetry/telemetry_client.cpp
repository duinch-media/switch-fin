#include "telemetry/telemetry_client.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

#include <borealis/core/logger.hpp>

#include "api/http.hpp"
#include "telemetry/health_snapshot.hpp"
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
    this->snapshotPath = configDir + "/health_current.json";
    // Runs at launch, never during playback: a leftover snapshot is a session that crashed.
    if (recoverSnapshot(this->snapshotPath, *this->queue)) {
        brls::Logger::info("Telemetry: recovered a session that did not finish cleanly");
    }
    brls::Logger::info("Telemetry: {} ({} queued from earlier runs)",
        this->config.enabled ? "reporting to " + this->config.endpoint : "no health.json, recording locally only",
        this->queue->size());
    if (this->config.enabled && this->queue->size() > 0) this->flushAsync();
}

void TelemetryClient::recordEvent(const TelemetryEvent& event) {
    if (!this->queue) return;
    try {
        TelemetryEvent ev = event;
        if (ev.session_id.empty()) ev.session_id = misc::randHex(16);
        nlohmann::json payload = buildHealthPayload(
            ev, "switchfin", AppVersion::getVersion(), platformName(), this->config.env, this->deviceId);
        this->queue->append(payload.dump());
        brls::Logger::info("Telemetry: queued event '{}' ({})", ev.event, ev.session_id);
    } catch (const std::exception& e) {
        brls::Logger::warning("Telemetry: cannot queue event '{}': {}", event.event, e.what());
    }
}

void TelemetryClient::recordAppStart(int64_t coldStartMs, bool appletMode, bool docked, int wifiStrength) {
    TelemetryEvent ev;
    ev.event = "app_start";
    ev.session_id = misc::randHex(16);
    std::string now = toIso8601();
    ev.ts_start = now;
    ev.ts_end = now;
    if (coldStartMs > 0) ev.metrics["cold_start_ms"] = coldStartMs;
    if (this->queue) {
        ev.metrics["queue_flushed_count"] = this->queue->size();
        ev.metrics["queue_dropped_count"] = this->queue->overflowDropped();
    }
    ev.context["applet_mode"] = appletMode;
    ev.context["docked"] = docked;
    if (wifiStrength >= 0) ev.context["wifi_strength"] = wifiStrength;
    this->recordEvent(ev);
    this->flushAsync();
}

void TelemetryClient::recordAppExit(int64_t uptimeMs, bool docked) {
    TelemetryEvent ev;
    ev.event = "app_exit";
    ev.end_reason = "app_exit";
    ev.session_id = misc::randHex(16);
    std::string now = toIso8601();
    ev.ts_start = now;
    ev.ts_end = now;
    if (uptimeMs > 0) ev.metrics["uptime_ms"] = uptimeMs;
    ev.context["docked"] = docked;
    ev.context["clean_exit"] = true;
    this->recordEvent(ev);
}

void TelemetryClient::recordScreenView(const std::string& screenName) {
    TelemetryEvent ev;
    ev.event = "screen_view";
    ev.session_id = misc::randHex(16);
    std::string now = toIso8601();
    ev.ts_start = now;
    ev.ts_end = now;
    ev.context["screen_name"] = screenName;
    this->recordEvent(ev);
}

void TelemetryClient::recordAppError(const std::string& component, const std::string& errorCode, const std::string& message) {
    TelemetryEvent ev;
    ev.event = "app_error";
    ev.session_id = misc::randHex(16);
    std::string now = toIso8601();
    ev.ts_start = now;
    ev.ts_end = now;
    ev.metrics["error_count"] = 1;
    ev.context["component"] = component;
    ev.context["error_code"] = errorCode;
    if (!message.empty()) ev.context["message"] = message.substr(0, 256);
    this->recordEvent(ev);
}

void TelemetryClient::recordPlaybackBuffer(const std::string& sessionId, int64_t bufferMs, int64_t posMs,
                                          int64_t bufferIndex, const std::string& trigger, bool docked,
                                          const std::string& codec, const std::string& resolution,
                                          int64_t bitrateKbps, int64_t inputKbps) {
    TelemetryEvent ev;
    ev.event = "playback_buffer";
    ev.session_id = sessionId.empty() ? misc::randHex(16) : (sessionId + "-b" + std::to_string(bufferIndex));
    std::string now = toIso8601();
    ev.ts_start = now;
    ev.ts_end = now;
    ev.metrics["buffer_ms"] = bufferMs;
    if (posMs >= 0) ev.metrics["pos_ms"] = posMs;
    ev.metrics["buffer_index"] = bufferIndex;
    if (inputKbps > 0) ev.metrics["input_kbps"] = inputKbps;

    ev.context["session_id"] = sessionId;
    ev.context["trigger"] = trigger;
    ev.context["docked"] = docked;
    if (!codec.empty()) ev.context["video_codec"] = codec;
    if (!resolution.empty()) ev.context["resolution"] = resolution;
    if (bitrateKbps > 0) ev.context["bitrate_kbps"] = bitrateKbps;
    this->recordEvent(ev);
}

void TelemetryClient::recordPlaybackSeek(const std::string& sessionId, int64_t seekMs, double fromPosSec,
                                        double toPosSec, int64_t seekIndex, bool docked,
                                        const std::string& codec, int64_t bitrateKbps) {
    TelemetryEvent ev;
    ev.event = "playback_seek";
    ev.session_id = sessionId.empty() ? misc::randHex(16) : (sessionId + "-s" + std::to_string(seekIndex));
    std::string now = toIso8601();
    ev.ts_start = now;
    ev.ts_end = now;
    ev.metrics["seek_ms"] = seekMs;
    if (fromPosSec >= 0) ev.metrics["from_pos_sec"] = fromPosSec;
    if (toPosSec >= 0) ev.metrics["to_pos_sec"] = toPosSec;
    ev.metrics["seek_index"] = seekIndex;

    ev.context["session_id"] = sessionId;
    ev.context["docked"] = docked;
    if (!codec.empty()) ev.context["video_codec"] = codec;
    if (bitrateKbps > 0) ev.context["bitrate_kbps"] = bitrateKbps;
    this->recordEvent(ev);
}

TelemetryEvent TelemetryClient::toTelemetryEvent(const PlaybackEvent& event) const {
    TelemetryEvent ev;
    ev.event = "playback_session";
    ev.session_id = event.session_id;
    ev.ts_start = event.ts_start;
    ev.ts_end = event.ts_end;
    ev.end_reason = event.end_reason;

    const auto& m = event.metrics;
    if (m.init_ms > 0) ev.metrics["init_ms"] = m.init_ms;
    if (m.api_ms > 0) ev.metrics["api_ms"] = m.api_ms;
    if (m.open_ms > 0) ev.metrics["open_ms"] = m.open_ms;
    if (m.ttff_ms > 0) ev.metrics["ttff_ms"] = m.ttff_ms;
    ev.metrics["play_ms"] = m.play_ms;
    ev.metrics["stall_count"] = m.stall_count;
    ev.metrics["stall_ms"] = m.stall_ms;
    ev.metrics["rebuffer_ratio"] = m.rebuffer_ratio;
    ev.metrics["seek_count"] = m.seek_count;
    if (m.seek_count > 0 && m.seek_p50_ms > 0) ev.metrics["seek_p50_ms"] = m.seek_p50_ms;
    if (m.input_kbps_avg > 0) ev.metrics["input_kbps_avg"] = m.input_kbps_avg;
    if (m.input_kbps_p10 > 0) ev.metrics["input_kbps_p10"] = m.input_kbps_p10;
    ev.metrics["dropped_frames"] = m.dropped_frames;

    const auto& c = event.context;
    if (!c.play_method.empty()) ev.context["play_method"] = c.play_method;
    if (!c.video_codec.empty()) ev.context["video_codec"] = c.video_codec;
    if (!c.hwdec.empty()) ev.context["hwdec"] = c.hwdec;
    if (!c.resolution.empty()) ev.context["resolution"] = c.resolution;
    if (c.bitrate_kbps > 0) ev.context["bitrate_kbps"] = c.bitrate_kbps;
    if (c.wifi_strength >= 0) ev.context["wifi_strength"] = c.wifi_strength;
    ev.context["docked"] = c.docked;
    return ev;
}

void TelemetryClient::recordPlayback(const PlaybackEvent& event) {
    if (!this->queue) return;
    try {
        TelemetryEvent ev = this->toTelemetryEvent(event);
        nlohmann::json payload = buildHealthPayload(
            ev, "switchfin", AppVersion::getVersion(), platformName(), this->config.env, this->deviceId);
        this->queue->append(payload.dump());
        clearSnapshot(this->snapshotPath);
        brls::Logger::info("Telemetry: queued playback session {}", event.session_id);
    } catch (const std::exception& e) {
        brls::Logger::warning("Telemetry: cannot queue event: {}", e.what());
    }
}

void TelemetryClient::saveSnapshot(const PlaybackEvent& event) {
    if (!this->queue) return;
    try {
        TelemetryEvent ev = this->toTelemetryEvent(event);
        nlohmann::json payload = buildHealthPayload(
            ev, "switchfin", AppVersion::getVersion(), platformName(), this->config.env, this->deviceId);
        writeSnapshot(this->snapshotPath, payload.dump());
    } catch (const std::exception& e) {
        brls::Logger::warning("Telemetry: cannot save snapshot: {}", e.what());
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
    if (this->busy.exchange(true)) return;
    const std::string url = this->config.endpoint;
    HTTP::Header headers = {
        "Content-Type: application/x-ndjson",
        "Authorization: Bearer " + this->config.apiKey,
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
