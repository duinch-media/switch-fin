#pragma once

#include "domain/error.hpp"
#include <string>
#include <cstdint>

namespace domain {

enum class PlaybackStatus {
    IDLE = 0,
    BUFFERING,
    PLAYING,
    PAUSED,
    STOPPED,
    COMPLETED,
    ERROR,
};

class IPlaybackService {
public:
    virtual ~IPlaybackService() = default;

    virtual Result<bool> loadMedia(const std::string& url, int64_t startPositionMs = 0) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seek(int64_t positionMs) = 0;
    virtual PlaybackStatus getStatus() const noexcept = 0;
    virtual int64_t getPositionMs() const noexcept = 0;
    virtual int64_t getDurationMs() const noexcept = 0;
};

} // namespace domain
