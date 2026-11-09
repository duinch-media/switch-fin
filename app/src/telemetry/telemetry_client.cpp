#include "telemetry/telemetry_client.hpp"
#include "api/http.hpp"
#include "utils/config.hpp"
#include "utils/misc.hpp"
#include "utils/thread.hpp"
#include <borealis/core/logger.hpp>
#include <regex>

namespace telemetry {

namespace {

const std::string DEFAULT_KEY = "ah_switchfin_0be7bd27206f93c85279c8bdea200ade555d3988a060783a";

}  // namespace

TelemetryClient& TelemetryClient::instance() {
    static TelemetryClient s_instance;
    return s_instance;
}

TelemetryClient::TelemetryClient() : apiKey(DEFAULT_KEY) {
    this->ticker.setCallback([this]() {
        ThreadPool::instance().submit([this](HTTP&) { this->sendBatch(); });
    });
    this->ticker.start(15000);  // Check queue every 15s
}

TelemetryClient::~TelemetryClient() {
    this->ticker.stop();
    this->flush();
}

void TelemetryClient::setGatewayUrl(const std::string& url) {
    this->gatewayUrl = url;
}

void TelemetryClient::setApiKey(const std::string& key) {
    this->apiKey = key;
}

std::string TelemetryClient::resolveGatewayUrl() const {
    if (!this->gatewayUrl.empty()) {
        return this->gatewayUrl;
    }

    std::string svr = AppConfig::instance().getUrl();
    if (!svr.empty()) {
        // e.g. http://192.168.1.37:8096 -> http://192.168.1.37:8089/v1/events
        std::regex re(R"((https?://[^:/]+)(?::\d+)?)");
        std::smatch match;
        if (std::regex_search(svr, match, re) && match.size() >= 2) {
            return match[1].str() + ":8089/v1/events";
        }
    }
    return "http://127.0.0.1:8089/v1/events";
}

std::string TelemetryClient::getFormattedDeviceId() {
    std::string raw = AppConfig::instance().getDeviceId();
    // Normalize to UUID format 8-4-4-4-12
    std::string clean;
    for (char c : raw) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
            clean += std::tolower(static_cast<unsigned char>(c));
        }
    }
    while (clean.length() < 32) clean += '0';
    if (clean.length() > 32) clean = clean.substr(0, 32);

    return clean.substr(0, 8) + "-" + clean.substr(8, 4) + "-" +
           clean.substr(12, 4) + "-" + clean.substr(16, 4) + "-" +
           clean.substr(20, 12);
}

nlohmann::json TelemetryClient::buildPayload(const PlaybackEvent& event) {
    nlohmann::json metrics = nlohmann::json::object();
    if (event.metrics.init_ms > 0) metrics["init_ms"] = event.metrics.init_ms;
    if (event.metrics.api_ms > 0) metrics["api_ms"] = event.metrics.api_ms;
    if (event.metrics.open_ms > 0) metrics["open_ms"] = event.metrics.open_ms;
    if (event.metrics.ttff_ms > 0) metrics["ttff_ms"] = event.metrics.ttff_ms;
    if (event.metrics.play_ms >= 0) metrics["play_ms"] = event.metrics.play_ms;
    metrics["stall_count"] = event.metrics.stall_count;
    metrics["stall_ms"] = event.metrics.stall_ms;
    metrics["rebuffer_ratio"] = event.metrics.rebuffer_ratio;
    metrics["seek_count"] = event.metrics.seek_count;
    if (event.metrics.seek_count > 0 && event.metrics.seek_p50_ms > 0) {
        metrics["seek_p50_ms"] = event.metrics.seek_p50_ms;
    }
    if (event.metrics.input_kbps_avg > 0) metrics["input_kbps_avg"] = event.metrics.input_kbps_avg;
    metrics["dropped_frames"] = event.metrics.dropped_frames;

    nlohmann::json context = nlohmann::json::object();
    if (!event.context.play_method.empty()) context["play_method"] = event.context.play_method;
    if (!event.context.video_codec.empty()) context["video_codec"] = event.context.video_codec;
    if (!event.context.hwdec.empty()) context["hwdec"] = event.context.hwdec;
    if (!event.context.resolution.empty()) context["resolution"] = event.context.resolution;
    if (event.context.bitrate_kbps > 0) context["bitrate_kbps"] = event.context.bitrate_kbps;
    if (event.context.wifi_strength >= 0) context["wifi_strength"] = event.context.wifi_strength;
    context["docked"] = event.context.docked;

    std::string platform = AppVersion::getPlatform();
    for (auto& c : platform) c = std::tolower(static_cast<unsigned char>(c));

    nlohmann::json envelope = {
        {"schema", "app-health/1"},
        {"event", "playback_session"},
        {"app", "switchfin"},
        {"app_version", AppVersion::getVersion()},
        {"platform", platform.empty() ? "switch" : platform},
        {"env", "prod"},
        {"device_id", this->getFormattedDeviceId()},
        {"session_id", event.session_id.empty() ? misc::randHex(16) : event.session_id},
        {"ts_start", event.ts_start},
        {"ts_end", event.ts_end},
        {"end_reason", event.end_reason},
        {"metrics", metrics},
        {"context", context}
    };

    return envelope;
}

void TelemetryClient::recordPlayback(const PlaybackEvent& event) {
    std::lock_guard<std::mutex> lock(this->queueMutex);
    this->queue.push_back(event);
    if (this->queue.size() > 500) {
        this->queue.pop_front();
    }
    brls::Logger::info("Telemetry: Queued playback session {}", event.session_id);
}

void TelemetryClient::flush() {
    this->sendBatch();
}

void TelemetryClient::sendBatch() {
    std::vector<PlaybackEvent> batch;
    {
        std::lock_guard<std::mutex> lock(this->queueMutex);
        if (this->queue.empty()) return;
        while (!this->queue.empty() && batch.size() < 20) {
            batch.push_back(this->queue.front());
            this->queue.pop_front();
        }
    }

    std::string url = this->resolveGatewayUrl();
    HTTP::Header headers = {
        "Content-Type: application/json",
        "X-API-Key: " + this->apiKey
    };

    for (const auto& ev : batch) {
        try {
            nlohmann::json payload = this->buildPayload(ev);
            std::string body = payload.dump();
            std::string resp = HTTP::post(url, body, headers, HTTP::Timeout{5000});
            brls::Logger::info("Telemetry: Sent session {} to {}: {}", ev.session_id, url, resp);
        } catch (const std::exception& e) {
            brls::Logger::warning("Telemetry send failed: {}", e.what());
            // Re-queue failed event once if queue is small
            std::lock_guard<std::mutex> lock(this->queueMutex);
            if (this->queue.size() < 200) {
                this->queue.push_back(ev);
            }
            break;
        }
    }
}

}  // namespace telemetry
