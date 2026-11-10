#include "telemetry/player_telemetry_hooks.hpp"

#include "view/mpv_core.hpp"

namespace telemetry {

namespace {

/// Network input rate in bytes/s from the demuxer, or 0 when unknown. cache-speed is the
/// cache fill rate and reads 0 once the cache is full, so it says nothing about the link.
int64_t demuxerInputRate(MPVCore& mpv) {
    if (mpv.getString("demuxer-cache-idle") != "no") return 0;  // idle: nothing is being fetched
    auto state = mpv.getNodeMap("demuxer-cache-state");
    auto it = state.find("raw-input-rate");
    if (it == state.end()) return 0;
    switch (it->second.format) {
    case MPV_FORMAT_INT64: return it->second.u.int64;
    case MPV_FORMAT_DOUBLE: return static_cast<int64_t>(it->second.u.double_);
    default: return 0;
    }
}

void sample(PlaybackTracker& tracker, MPVCore& mpv) {
    if (!tracker.sampleDue()) return;
    tracker.sampleSpeed(demuxerInputRate(mpv));
    tracker.sampleDroppedFrames(mpv.getInt("decoder-frame-drop-count") + mpv.getInt("frame-drop-count"));
}

}  // namespace

void startPlayback(PlaybackTracker& tracker, const std::string& sessionId, const std::string& playMethod,
                   const jellyfin::Source& source, int64_t apiMs) {
    jellyfin::Stream video;
    for (const auto& s : source.MediaStreams) {
        if (s.Type == jellyfin::streamTypeVideo) {
            video = s;
            break;
        }
    }
    tracker.start(sessionId, playMethod, video, source.Bitrate, MPVCore::instance().getInitMs());
    tracker.markApiTime(apiMs);
}

void onMpvEvent(PlaybackTracker& tracker, MpvEventEnum event) {
    auto& mpv = MPVCore::instance();
    switch (event) {
    case MpvEventEnum::START_FILE: tracker.onStartFile(); break;
    case MpvEventEnum::MPV_LOADED: tracker.onLoaded(); break;
    case MpvEventEnum::LOADING_START: tracker.onLoadingStart(); break;
    case MpvEventEnum::LOADING_END:
        if (tracker.awaitingFirstFrame()) {
            tracker.onFirstFrame(mpv.getString("hwdec-current"));
        } else {
            tracker.onLoadingEnd();
        }
        break;
    case MpvEventEnum::MPV_PAUSE: tracker.onPause(); break;
    case MpvEventEnum::MPV_RESUME: tracker.onResume(); break;
    case MpvEventEnum::MPV_SEEK_START: tracker.onSeekStart(); break;
    case MpvEventEnum::MPV_SEEK_END: tracker.onSeekEnd(); break;
    case MpvEventEnum::MPV_BUFFERING_START: tracker.onStallStart(); break;
    case MpvEventEnum::MPV_BUFFERING_END: tracker.onStallEnd(); break;
    case MpvEventEnum::CACHE_SPEED_CHANGE: sample(tracker, mpv); break;
    case MpvEventEnum::MPV_STOP: tracker.finish("user_exit"); break;
    case MpvEventEnum::MPV_FILE_ERROR:
        tracker.finish("error");
        TelemetryClient::instance().recordAppError("player", "mpv_file_error", "Playback terminated due to file error");
        break;
    case MpvEventEnum::END_OF_FILE: tracker.finish("eof"); break;
    default:;
    }
}

}  // namespace telemetry
