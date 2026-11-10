#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

#include "telemetry/health_config.hpp"
#include "telemetry/health_queue.hpp"
#include "telemetry/telemetry_event.hpp"

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
    int64_t input_kbps_p10 = 0;
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

/// Client side of the app-health contract (see the app-health repo, docs/schema.md):
/// events are queued on disk and submitted in batches only when the player is left,
/// when the app exits, and (for leftovers) at the next launch.
class TelemetryClient {
public:
    static TelemetryClient& instance();

    /// Load health.json and the install id, open the queue, then retry leftovers
    /// from earlier runs in the background. Safe to call once at startup.
    void init(const std::string& configDir);

    /// Low-level queueing for any event adhering to app-health/1 envelope.
    void recordEvent(const TelemetryEvent& event);

    /// App lifecycle events
    void recordAppStart(int64_t coldStartMs, bool appletMode, bool docked, int wifiStrength = -1);
    void recordAppExit(int64_t uptimeMs, bool docked);

    /// Screen navigation & operational errors
    void recordScreenView(const std::string& screenName);
    void recordAppError(const std::string& component, const std::string& errorCode, const std::string& message = "");

    /// Discrete playback sub-events (buffer stall, user seek)
    void recordPlaybackBuffer(const std::string& sessionId, int64_t bufferMs, int64_t posMs,
                              int64_t bufferIndex, const std::string& trigger, bool docked,
                              const std::string& codec, const std::string& resolution,
                              int64_t bitrateKbps, int64_t inputKbps);
    void recordPlaybackSeek(const std::string& sessionId, int64_t seekMs, double fromPosSec,
                            double toPosSec, int64_t seekIndex, bool docked,
                            const std::string& codec, int64_t bitrateKbps);

    /// Queue a finished playback session. Never touches the network.
    void recordPlayback(const PlaybackEvent& event);

    /// Persist an in-progress session so a crash can still be reported at the next launch.
    void saveSnapshot(const PlaybackEvent& event);

    /// Leave-player trigger: submit the queue on a worker thread.
    void flushAsync();

    /// App-exit trigger: synchronous, short deadline.
    void shutdown();

private:
    TelemetryClient() = default;

    TelemetryEvent toTelemetryEvent(const PlaybackEvent& event) const;
    void drain(long timeoutMs);

    HealthConfig config;
    std::string deviceId;
    std::unique_ptr<HealthQueue> queue;
    std::string snapshotPath;
    std::atomic<bool> busy{false};
};

}  // namespace telemetry
