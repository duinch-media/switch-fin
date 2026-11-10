#include "telemetry/playback_tracker.hpp"
#include <ctime>
#include <algorithm>
#include <numeric>
#include <borealis/core/logger.hpp>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace telemetry {

namespace {

constexpr auto kSnapshotInterval = std::chrono::seconds(30);
constexpr auto kSampleInterval = std::chrono::seconds(1);

int64_t msSince(const std::chrono::steady_clock::time_point& from,
                const std::chrono::steady_clock::time_point& to = std::chrono::steady_clock::now()) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(to - from).count();
}

#ifdef __SWITCH__
/// Wi-Fi signal bars (0-3), or -1 when not on Wi-Fi or the call fails.
int wifiStrength() {
    NifmInternetConnectionType type;
    u32 strength = 0;
    NifmInternetConnectionStatus status;
    if (R_FAILED(nifmGetInternetConnectionStatus(&type, &strength, &status))) return -1;
    if (type != NifmInternetConnectionType_WiFi || strength > 3) return -1;
    return static_cast<int>(strength);
}
#endif

}  // namespace

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
                            const jellyfin::Stream& videoStream, int64_t bitrate, int64_t initMs) {
    this->active = true;
    this->hasFirstFrame = false;
    this->inStall = false;
    this->inPause = false;
    this->inSeek = false;
    this->totalPauseMs = 0;
    this->speedSamplesKbps.clear();
    this->seekDurations.clear();
    this->lastSampleAt = {};
    this->lastSnapshotAt = {};

    this->wallStart = std::chrono::system_clock::now();
    this->steadyStart = std::chrono::steady_clock::now();

    this->event = PlaybackEvent();
    this->event.session_id = sessionId;
    this->event.ts_start = this->toRfc3339(this->wallStart);

    this->event.metrics.init_ms = initMs;
    this->event.context.play_method = playMethod;
    this->event.context.video_codec = videoStream.Codec;
    if (!videoStream.DisplayTitle.empty()) {
        this->event.context.resolution = videoStream.DisplayTitle;
    }
    this->event.context.bitrate_kbps = bitrate > 0 ? (bitrate / 1000) : 0;

#ifdef __SWITCH__
    this->event.context.docked = (appletGetOperationMode() != AppletOperationMode_Handheld);
    this->event.context.wifi_strength = wifiStrength();
#endif

    brls::Logger::info("Telemetry: Started tracking playback session {}", sessionId);
    this->persist(true);
}

void PlaybackTracker::markApiTime(int64_t ms) {
    this->event.metrics.api_ms = ms;
}

void PlaybackTracker::onStartFile() {
    this->startFileAt = std::chrono::steady_clock::now();
    this->hasStartFile = true;
}

void PlaybackTracker::onLoaded() {
    if (!this->active || !this->hasStartFile) return;
    this->hasStartFile = false;
    this->event.metrics.open_ms = msSince(this->startFileAt);
}

void PlaybackTracker::onFirstFrame(const std::string& hwdec) {
    if (!this->hasFirstFrame && this->active) {
        this->hasFirstFrame = true;
        this->event.context.hwdec = hwdec;
        this->event.metrics.ttff_ms = msSince(this->steadyStart);
        brls::Logger::info("Telemetry: First frame rendered, TTFF = {} ms", this->event.metrics.ttff_ms);
    }
}

void PlaybackTracker::onPause() {
    if (this->active && !this->inPause) {
        this->inPause = true;
        this->pauseStart = std::chrono::steady_clock::now();
        this->persist(true);
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
    // Seeks before the first frame are mpv's own start-position seek, not the user's.
    if (this->active && this->hasFirstFrame && !this->inSeek) {
        if (this->inStall) this->onStallEnd();  // the wait after a seek is a seek wait, not a stall
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

bool PlaybackTracker::sampleDue() {
    if (!this->active || !this->hasFirstFrame) return false;
    auto now = std::chrono::steady_clock::now();
    if (this->lastSampleAt != std::chrono::steady_clock::time_point{} && now - this->lastSampleAt < kSampleInterval)
        return false;
    this->lastSampleAt = now;
    return true;
}

void PlaybackTracker::sampleSpeed(int64_t bytesPerSec) {
    if (this->active && bytesPerSec > 0) {
        this->speedSamplesKbps.push_back((bytesPerSec * 8) / 1000);
    }
    this->persist(false);
}

void PlaybackTracker::sampleDroppedFrames(int64_t count) {
    if (this->active) {
        this->event.metrics.dropped_frames = std::max(this->event.metrics.dropped_frames, count);
    }
}

void PlaybackTracker::persist(bool force) {
    if (!this->active) return;
    auto now = std::chrono::steady_clock::now();
    if (!force && now - this->lastSnapshotAt < kSnapshotInterval) return;
    this->lastSnapshotAt = now;
    TelemetryClient::instance().saveSnapshot(this->buildEvent("crash"));
}

PlaybackEvent PlaybackTracker::buildEvent(const std::string& reason) const {
    auto now = std::chrono::steady_clock::now();
    PlaybackEvent e = this->event;
    e.end_reason = reason;
    e.ts_end = toRfc3339(std::chrono::system_clock::now());

    // Open stall/pause intervals count up to now (a live snapshot may be taken mid-stall).
    int64_t pauseMs = this->totalPauseMs + (this->inPause ? msSince(this->pauseStart, now) : 0);
    if (this->inStall) e.metrics.stall_ms += msSince(this->stallStart, now);

    e.metrics.play_ms = std::max<int64_t>(0, msSince(this->steadyStart, now) - e.metrics.stall_ms - pauseMs);
    int64_t denom = e.metrics.play_ms + e.metrics.stall_ms;
    if (denom > 0) e.metrics.rebuffer_ratio = (double)e.metrics.stall_ms / (double)denom;

    if (!this->speedSamplesKbps.empty()) {
        auto v = this->speedSamplesKbps;
        std::sort(v.begin(), v.end());
        e.metrics.input_kbps_avg = std::accumulate(v.begin(), v.end(), int64_t{0}) / (int64_t)v.size();
        e.metrics.input_kbps_p10 = v[(v.size() - 1) / 10];  // slow tail
    }

    if (!this->seekDurations.empty()) {
        auto d = this->seekDurations;
        std::sort(d.begin(), d.end());
        e.metrics.seek_p50_ms = d[d.size() / 2];
    }
    return e;
}

void PlaybackTracker::finish(const std::string& reason) {
    if (!this->active) return;
    if (this->inStall) this->onStallEnd();
    if (this->inPause) this->onResume();
    this->active = false;

    PlaybackEvent done = this->buildEvent(reason);
    brls::Logger::info("Telemetry: Finished session {} (play: {} ms, stalls: {}, stall_ms: {}, reason: {})",
                       done.session_id, done.metrics.play_ms, done.metrics.stall_count,
                       done.metrics.stall_ms, reason);

    TelemetryClient::instance().recordPlayback(done);
    TelemetryClient::instance().flushAsync();
}

}  // namespace telemetry
