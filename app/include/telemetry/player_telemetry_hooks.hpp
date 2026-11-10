#pragma once

#include <string>

#include "api/jellyfin.hpp"
#include "telemetry/playback_tracker.hpp"
#include "utils/event.hpp"

namespace telemetry {

/// Begin tracking a session: finds the video stream, adds the mpv init time and the
/// PlaybackInfo request time (apiMs, applied after start() because start resets the event).
void startPlayback(PlaybackTracker& tracker, const std::string& sessionId, const std::string& playMethod,
                   const jellyfin::Source& source, int64_t apiMs);

/// Map an mpv event onto the tracker. Call it first in the player's event handler.
void onMpvEvent(PlaybackTracker& tracker, MpvEventEnum event);

}  // namespace telemetry
