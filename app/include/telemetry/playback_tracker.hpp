#pragma once

#include "telemetry/telemetry_client.hpp"
#include "api/jellyfin.hpp"
#include <chrono>
#include <vector>

namespace telemetry {

class PlaybackTracker {
public:
    PlaybackTracker();
    ~PlaybackTracker();

    void start(const std::string& sessionId, const std::string& playMethod,
               const jellyfin::Stream& videoStream, int64_t bitrate, int64_t initMs = 0);
    void markApiTime(int64_t ms);
    void onStartFile();
    void onLoaded();
    bool awaitingFirstFrame() const { return active && !hasFirstFrame; }
    void onFirstFrame(const std::string& hwdec);
    void onPause();
    void onResume();
    void onStallStart();
    void onStallEnd();
    void onLoadingStart();
    void onLoadingEnd();
    void onSeekStart();
    void onSeekEnd();
    /// True at most once per second while playing: gate for the (costly) mpv property reads.
    bool sampleDue();
    void sampleSpeed(int64_t bytesPerSec);
    void sampleDroppedFrames(int64_t count);
    void finish(const std::string& reason = "user_exit");

    bool isPlaying() const { return active; }

private:
    static std::string toRfc3339(const std::chrono::system_clock::time_point& tp);
    PlaybackEvent buildEvent(const std::string& reason) const;
    /// Persist an in-progress snapshot for crash recovery (throttled unless forced).
    void persist(bool force);

    bool active = false;
    bool hasFirstFrame = false;
    bool inStall = false;
    bool inPause = false;
    bool inSeek = false;
    bool hasStartFile = false;

    std::chrono::system_clock::time_point wallStart;
    std::chrono::steady_clock::time_point steadyStart;
    std::chrono::steady_clock::time_point stallStart;
    std::chrono::steady_clock::time_point pauseStart;
    std::chrono::steady_clock::time_point seekStart;
    std::chrono::steady_clock::time_point startFileAt;
    std::chrono::steady_clock::time_point lastSampleAt;
    std::chrono::steady_clock::time_point lastSnapshotAt;

    int64_t totalPauseMs = 0;
    int64_t stallPosMs = 0;
    std::string stallTrigger = "underrun";
    double seekFromPosSec = 0.0;

    std::vector<int64_t> speedSamplesKbps;
    std::vector<int64_t> seekDurations;

    PlaybackEvent event;
};

}  // namespace telemetry
