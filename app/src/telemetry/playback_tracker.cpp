#include "telemetry/playback_tracker.hpp"
#include <ctime>
#include <algorithm>
#include <numeric>
#include <borealis/core/logger.hpp>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace telemetry {

PlaybackTracker::PlaybackTracker() = default;
PlaybackTracker::~PlaybackTracker() {
    if (this->active) {
        this->finish("user_exit");
    }
}

std::string PlaybackTracker::toRfc3339(const std::chrono::system_clock::time_point& tp) {
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

void PlaybackTracker::start(const std::string& sessionId, const std::string& playMethod,
                            const jellyfin::Stream& videoStream, int64_t bitrate) {
    this->active = true;
    this->hasFirstFrame = false;
    this->inStall = false;
    this->inPause = false;
    this->inSeek = false;
    this->totalPauseMs = 0;
    this->speedSamplesCount = 0;
    this->speedSumKbps = 0;
    this->seekDurations.clear();

    this->wallStart = std::chrono::system_clock::now();
    this->steadyStart = std::chrono::steady_clock::now();

    this->event = PlaybackEvent();
    this->event.session_id = sessionId;
    this->event.ts_start = this->toRfc3339(this->wallStart);

    this->event.context.play_method = playMethod;
    this->event.context.video_codec = videoStream.Codec;
    if (!videoStream.DisplayTitle.empty()) {
        this->event.context.resolution = videoStream.DisplayTitle;
    }
    this->event.context.bitrate_kbps = bitrate > 0 ? (bitrate / 1000) : 0;

#ifdef __SWITCH__
    this->event.context.docked = (appletGetOperationMode() != AppletOperationMode_Handheld);
    this->event.context.hwdec = "nvtegra";
#else
    this->event.context.docked = false;
    this->event.context.hwdec = "auto";
#endif

    brls::Logger::info("Telemetry: Started tracking playback session {}", sessionId);
}

void PlaybackTracker::markApiTime(int64_t ms) {
    this->event.metrics.api_ms = ms;
}

void PlaybackTracker::onFirstFrame() {
    if (!this->hasFirstFrame && this->active) {
        this->hasFirstFrame = true;
        auto now = std::chrono::steady_clock::now();
        this->event.metrics.ttff_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - this->steadyStart).count();
        brls::Logger::info("Telemetry: First frame rendered, TTFF = {} ms", this->event.metrics.ttff_ms);
    }
}

void PlaybackTracker::onPause() {
    if (this->active && !this->inPause) {
        this->inPause = true;
        this->pauseStart = std::chrono::steady_clock::now();
    }
}

void PlaybackTracker::onResume() {
    if (this->active && this->inPause) {
        this->inPause = false;
        auto now = std::chrono::steady_clock::now();
        this->totalPauseMs +=
            std::chrono::duration_cast<std::chrono::milliseconds>(now - this->pauseStart).count();
    }
}

void PlaybackTracker::onStallStart() {
    // Only count as stall if first frame already appeared and we are not in pause or user seek
    if (this->active && this->hasFirstFrame && !this->inPause && !this->inSeek && !this->inStall) {
        this->inStall = true;
        this->event.metrics.stall_count++;
        this->stallStart = std::chrono::steady_clock::now();
        brls::Logger::info("Telemetry: Stall #{} started", this->event.metrics.stall_count);
    }
}

void PlaybackTracker::onStallEnd() {
    if (this->active && this->inStall) {
        this->inStall = false;
        auto now = std::chrono::steady_clock::now();
        int64_t duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - this->stallStart).count();
        this->event.metrics.stall_ms += duration;
        brls::Logger::info("Telemetry: Stall ended (duration: {} ms, total: {} ms)",
                           duration, this->event.metrics.stall_ms);
    }
}

void PlaybackTracker::onSeekStart() {
    if (this->active && !this->inSeek) {
        this->inSeek = true;
        this->event.metrics.seek_count++;
        this->seekStart = std::chrono::steady_clock::now();
    }
}

void PlaybackTracker::onSeekEnd() {
    if (this->active && this->inSeek) {
        this->inSeek = false;
        auto now = std::chrono::steady_clock::now();
        int64_t dur =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - this->seekStart).count();
        this->seekDurations.push_back(dur);
    }
}

void PlaybackTracker::sampleSpeed(int64_t bytesPerSec) {
    if (this->active && bytesPerSec > 0) {
        int64_t kbps = (bytesPerSec * 8) / 1000;
        this->speedSumKbps += kbps;
        this->speedSamplesCount++;
    }
}

void PlaybackTracker::sampleDroppedFrames(int64_t count) {
    if (this->active) {
        this->event.metrics.dropped_frames = count;
    }
}

void PlaybackTracker::finish(const std::string& reason) {
    if (!this->active) return;
    this->active = false;

    if (this->inStall) {
        this->onStallEnd();
    }
    if (this->inPause) {
        this->onResume();
    }

    this->event.end_reason = reason;
    this->event.ts_end = this->toRfc3339(std::chrono::system_clock::now());

    auto nowSteady = std::chrono::steady_clock::now();
    int64_t totalElapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(nowSteady - this->steadyStart).count();

    this->event.metrics.play_ms =
        std::max<int64_t>(0, totalElapsedMs - this->event.metrics.stall_ms - this->totalPauseMs);

    int64_t denom = this->event.metrics.play_ms + this->event.metrics.stall_ms;
    if (denom > 0) {
        this->event.metrics.rebuffer_ratio = (double)this->event.metrics.stall_ms / (double)denom;
    }

    if (this->speedSamplesCount > 0) {
        this->event.metrics.input_kbps_avg = this->speedSumKbps / this->speedSamplesCount;
    }

    if (!this->seekDurations.empty()) {
        std::sort(this->seekDurations.begin(), this->seekDurations.end());
        this->event.metrics.seek_p50_ms = this->seekDurations[this->seekDurations.size() / 2];
    }

    brls::Logger::info("Telemetry: Finished session {} (play: {} ms, stalls: {}, stall_ms: {}, reason: {})",
                       this->event.session_id, this->event.metrics.play_ms,
                       this->event.metrics.stall_count, this->event.metrics.stall_ms, reason);

    TelemetryClient::instance().recordPlayback(this->event);
    TelemetryClient::instance().flush();
}

}  // namespace telemetry
