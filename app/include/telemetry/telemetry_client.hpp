#pragma once

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <chrono>
#include <nlohmann/json.hpp>
#include <borealis/core/timer.hpp>
#include "utils/thread.hpp"

namespace telemetry {

struct PlaybackMetrics {
    int64_t init_ms = 0;
    int64_t api_ms = 0;
    int64_t open_ms = 0;
    int64_t ttff_ms = 0;
    int64_t play_ms = 0;
    int64_t stall_count = 0;
    int64_t stall_ms = 0;
    double rebuffer_ratio = 0.0;
    int64_t seek_count = 0;
    int64_t seek_p50_ms = 0;
    int64_t input_kbps_avg = 0;
    int64_t dropped_frames = 0;
};

struct PlaybackContext {
    std::string play_method;
    std::string video_codec;
    std::string hwdec;
    std::string resolution;
    int64_t bitrate_kbps = 0;
    int wifi_strength = -1;
    bool docked = false;
};

struct PlaybackEvent {
    std::string session_id;
    std::string ts_start;
    std::string ts_end;
    std::string end_reason = "user_exit";
    PlaybackMetrics metrics;
    PlaybackContext context;
};

class TelemetryClient {
public:
    static TelemetryClient& instance();

    void recordPlayback(const PlaybackEvent& event);
    void flush();
    void setGatewayUrl(const std::string& url);
    void setApiKey(const std::string& key);

private:
    TelemetryClient();
    ~TelemetryClient();

    std::string getFormattedDeviceId();
    std::string resolveGatewayUrl() const;
    nlohmann::json buildPayload(const PlaybackEvent& event);
    void sendBatch();

    std::string gatewayUrl;
    std::string apiKey;
    std::mutex queueMutex;
    std::deque<PlaybackEvent> queue;
    brls::RepeatingTimer ticker;
};

}  // namespace telemetry
