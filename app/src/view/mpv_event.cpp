#include "view/mpv_core.hpp"
#include <fmt/ranges.h>

std::string MPVCore::getCacheSpeed() const {
    if (cache_speed >> 20 > 0) {
        return fmt::format("{:.2f} MB/s", (cache_speed >> 10) / 1024.0f);
    } else if (cache_speed >> 10 > 0) {
        return fmt::format("{:.2f} KB/s", cache_speed / 1024.0f);
    } else {
        return fmt::format("{} B/s", cache_speed);
    }
}

void MPVCore::eventMainLoop() {
    while (true) {
        mpv_event *event = mpv_wait_event(this->mpv, 0);
        switch (event->event_id) {
        case MPV_EVENT_NONE:
            return;
        case MPV_EVENT_LOG_MESSAGE: {
            auto log = reinterpret_cast<mpv_event_log_message *>(event->data);
            std::string text = log->text;
            while (!text.empty() && text.back() == '\n') text.pop_back();
            if (log->log_level <= MPV_LOG_LEVEL_ERROR) {
                brls::Logger::error("{}: {}", log->prefix, text);
            } else if (log->log_level <= MPV_LOG_LEVEL_WARN) {
                brls::Logger::warning("{}: {}", log->prefix, text);
            } else if (log->log_level <= MPV_LOG_LEVEL_INFO) {
                brls::Logger::info("{}: {}", log->prefix, text);
            } else if (log->log_level <= MPV_LOG_LEVEL_V) {
                brls::Logger::debug("{}: {}", log->prefix, text);
            } else {
                brls::Logger::verbose("{}: {}", log->prefix, text);
            }
            break;
        }
        case MPV_EVENT_SHUTDOWN:
            brls::Logger::debug("MPVCore => EVENT_SHUTDOWN");
            return;
        case MPV_EVENT_FILE_LOADED:
            brls::Logger::info("MPVCore => EVENT_FILE_LOADED");
            mpvCoreEvent.fire(MpvEventEnum::MPV_LOADED);
            break;
        case MPV_EVENT_START_FILE:
            brls::Logger::info("MPVCore => EVENT_START_FILE");
            mpvCoreEvent.fire(MpvEventEnum::START_FILE);
            mpvCoreEvent.fire(MpvEventEnum::LOADING_START);
            break;
        case MPV_EVENT_PLAYBACK_RESTART:
            brls::Logger::info("MPVCore => EVENT_PLAYBACK_RESTART");
            this->video_stopped = false;
            if (this->isPaused())
                mpvCoreEvent.fire(MpvEventEnum::MPV_PAUSE);
            else
                mpvCoreEvent.fire(MpvEventEnum::MPV_RESUME);
            break;
        case MPV_EVENT_END_FILE: {
            this->video_stopped = true;
            auto node = (mpv_event_end_file *)event->data;
            if (node->reason == MPV_END_FILE_REASON_ERROR) {
                brls::Logger::error("MPVCore => FILE ERROR: {}", mpv_error_string(node->error));
                this->stop();
                mpvCoreEvent.fire(MpvEventEnum::MPV_FILE_ERROR);
            } else if (node->reason == MPV_END_FILE_REASON_EOF) {
                brls::Logger::info("MPVCore => END_OF_FILE");
                mpvCoreEvent.fire(MpvEventEnum::END_OF_FILE);
            } else {
                brls::Logger::info("MPVCore => STOP");
                mpvCoreEvent.fire(MpvEventEnum::MPV_STOP);
            }
            break;
        }
        case MPV_EVENT_COMMAND_REPLY: {
            mpv_event_command *cmd = (mpv_event_command *)event->data;
            if (event->error) {
                brls::Logger::error("MPVCore => COMMAND ERROR: {}", mpv_error_string(event->error));
                break;
            }
            if (event->reply_userdata > 0 && cmd->result.format == MPV_FORMAT_NODE_MAP) {
                mpv_node_list *node_list = cmd->result.u.list;
                for (int i = 0; i < node_list->num; i++) {
                    std::string key = node_list->keys[i];
                    auto &value = node_list->values[i];
                    if (key == "playlist_entry_id" && value.format == MPV_FORMAT_INT64) {
                        this->mpvCommandReply.fire(event->reply_userdata, value.u.int64);
                    }
                }
            }
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE: {
            mpv_event_property *prop = (mpv_event_property *)event->data;
            if (prop->format == MPV_FORMAT_NONE) break;

            switch (event->reply_userdata) {
            case 1:  // core-idle
                if (!*(int *)prop->data) {
                    mpvCoreEvent.fire(MpvEventEnum::LOADING_END);
                } else if (!this->video_stopped) {
                    brls::Logger::debug("MPVCore => IDLE");
                    mpvCoreEvent.fire(MpvEventEnum::LOADING_START);
                }
                break;
            case 2:  // pause
                this->video_paused = *(int *)prop->data;
                if (this->video_paused) {
                    brls::Logger::info("MPVCore => PAUSE");
                    mpvCoreEvent.fire(MpvEventEnum::MPV_PAUSE);
                } else if (!this->video_stopped) {
                    brls::Logger::info("MPVCore => RESUME");
                    mpvCoreEvent.fire(MpvEventEnum::MPV_RESUME);
                }
                break;
            case 3:  // duration
                duration = *(int64_t *)prop->data;
                if (duration >= 0) {
                    brls::Logger::debug("MPVCore => DURATION: {}", duration);
                    mpvCoreEvent.fire(MpvEventEnum::UPDATE_DURATION);
                }
                break;
            case 4:  // playback-time
                this->playback_time = *(double *)prop->data;
                if (video_progress != (int64_t)playback_time) {
                    video_progress = (int64_t)playback_time;
                    mpvCoreEvent.fire(MpvEventEnum::UPDATE_PROGRESS);
                }
                break;
            case 5:  // cache-speed
                this->cache_speed = *(int64_t *)prop->data;
                mpvCoreEvent.fire(MpvEventEnum::CACHE_SPEED_CHANGE);
                break;
            case 9:  // speed
                this->video_speed = *(double *)prop->data;
                mpvCoreEvent.fire(VIDEO_SPEED_CHANGE);
                break;
            case 10:  // volume
                if (*(int64_t *)prop->data > 0 && this->volume == 0) {
                    mpvCoreEvent.fire(VIDEO_UNMUTE);
                } else if (*(int64_t *)prop->data == 0 && this->volume > 0) {
                    mpvCoreEvent.fire(VIDEO_MUTE);
                }
                this->volume = *(int64_t *)prop->data;
                break;
            default:
                brls::Logger::debug("MPVCore => PROPERTY_CHANGE `{}` type {}", prop->name, int(prop->format));
            }
            break;
        }
        default:;
        }
    }
}

void MPVCore::reset() {
    brls::Logger::debug("MPVCore::reset");
    mpvCoreEvent.fire(MpvEventEnum::RESET);
    this->stop();
    this->video_stopped = true;
    this->video_paused = false;
    this->duration = 0;
    this->cache_speed = 0;
    this->playback_time = 0;
    this->video_progress = 0;

    setFrameSize(rect);
}

void MPVCore::setUrl(const std::string &url, const std::string &extra, const std::string &method, uint64_t userdata) {
    brls::Logger::debug("MPVCore {} ({}) extra: ({})", method, url, extra);
    if (mpv_client_api_version() >= MPV_MAKE_VERSION(2, 3)) {
        const char *cmd[] = {"loadfile", url.c_str(), method.c_str(), "0", extra.c_str(), nullptr};
        mpv_command_async(this->mpv, userdata, cmd);
    } else {
        const char *cmd[] = {"loadfile", url.c_str(), method.c_str(), extra.c_str(), nullptr};
        mpv_command_async(this->mpv, userdata, cmd);
    }
}

void MPVCore::togglePlay() { this->command("cycle", "pause"); }

void MPVCore::stop() { this->command("stop"); }

void MPVCore::seek(double value, const std::string &flags) {
    std::string pos = std::to_string(value);
    this->command("seek", pos.c_str(), flags.c_str());
}

bool MPVCore::isStopped() const { return video_stopped; }

bool MPVCore::isPaused() { return video_paused; }

double MPVCore::getSpeed() const { return video_speed; }

void MPVCore::setSpeed(double value) {
    std::string speed = std::to_string(value);
    this->command("set", "speed", speed.c_str());
}

void MPVCore::enableVO(bool value) { mpv_set_option_string(mpv, "vo", value ? MPVCore::VO.c_str() : "null"); }

void MPVCore::setAspect(const std::string &value) {
    if (value == "auto") {
        this->command("set", "keepaspect", "yes");
        this->command("set", "video-aspect-override", "no");
        this->command("set", "panscan", "0.0");
    } else if (value == "stretch") {
        this->command("set", "keepaspect", "no");
        this->command("set", "video-aspect-override", "-1");
        this->command("set", "panscan", "0.0");
    } else if (value == "crop") {
        this->command("set", "keepaspect", "yes");
        this->command("set", "video-aspect-override", "-1");
        this->command("set", "panscan", "1.0");
    } else if (!value.empty()) {
        this->command("set", "keepaspect", "yes");
        this->command("set", "video-aspect-override", value.c_str());
        this->command("set", "panscan", "0.0");
    }
}

std::string MPVCore::getString(const std::string &key) {
    char *value = nullptr;
    mpv_get_property(mpv, key.c_str(), MPV_FORMAT_STRING, &value);
    if (!value) return "";
    std::string result = std::string{value};
    mpv_free(value);
    return result;
}

double MPVCore::getDouble(const std::string &key) {
    double value = 0;
    mpv_get_property(mpv, key.c_str(), MPV_FORMAT_DOUBLE, &value);
    return value;
}

void MPVCore::setDouble(const std::string &key, double value) {
    mpv_set_property_async(mpv, 0, key.c_str(), MPV_FORMAT_DOUBLE, &value);
}

int64_t MPVCore::getInt(const std::string &key, int64_t default_value) {
    int64_t value = default_value;
    mpv_get_property(mpv, key.c_str(), MPV_FORMAT_INT64, &value);
    return value;
}

void MPVCore::setInt(const std::string &key, int64_t value) {
    mpv_set_property_async(mpv, 0, key.c_str(), MPV_FORMAT_INT64, &value);
}

std::unordered_map<std::string, mpv_node> MPVCore::getNodeMap(const std::string &key) {
    mpv_node node;
    std::unordered_map<std::string, mpv_node> nodeMap;
    if (mpv_get_property(mpv, key.c_str(), MPV_FORMAT_NODE, &node) >= 0) {
        if (node.format == MPV_FORMAT_NODE_MAP) {
            mpv_node_list *node_list = node.u.list;
            for (int i = 0; i < node_list->num; i++) {
                std::string key = node_list->keys[i];
                mpv_node &value = node_list->values[i];
                nodeMap.insert(std::make_pair(key, value));
            }
        }
        mpv_free_node_contents(&node);
    }
    return nodeMap;
}
