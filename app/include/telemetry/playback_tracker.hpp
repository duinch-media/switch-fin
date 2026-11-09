#pragma once

#include "telemetry/telemetry_client.hpp"
#include "api/jellyfin.hpp"
#include <chrono>

namespace telemetry {

class PlaybackTracker {
public:
    PlaybackTracker();
    ~PlaybackTracker();

    void start(const std::string& sessionId, const std::string& playMethod,
               const jellyfin::Stream& videoStream, int64_t bitrate);
    void markApiTime(int64_t ms);
    void onFirstFrame();
    void onPause();
    void onResume();
    void onStallStart();
    void onStallEnd();
    void onSeekStart();
    void onSeekEnd();
    void sampleSpeed(int64_t bytesPerSec);
    void sampleDroppedFrames(int64_t count);
    void finish(const std::string& reason = "user_exit");

    bool isPlaying() const { return active; }

private:
    std::string toRfc3339(const std::chrono::system_clock::time_point& tp);

    bool active = false;
    bool hasFirstFrame = false;
    bool inStall = false;
    bool inPause = false;
    bool inSeek = false;

    std::chrono::system_clock::time_point wallStart;
    std::chrono::steady_clock::time_point steadyStart;
    std::chrono::steady_clock::time_point stallStart;
    std::chrono::steady_clock::time_point pauseStart;
    std::chrono::steady_clock::time_point seekStart;

    int64_t totalPauseMs = 0;
    int64_t speedSamplesCount = 0;
    int64_t speedSumKbps = 0;
    std::vector<int64_t> seekDurations;

    PlaybackEvent event;
};

}  // namespace telemetry
