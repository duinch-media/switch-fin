#include "activity/player_view.hpp"
#include "api/jellyfin.hpp"
#include "utils/dialog.hpp"
#include "utils/misc.hpp"
#include "view/danmaku_core.hpp"
#include "view/mpv_core.hpp"
#include "view/player_setting.hpp"
#include "view/video_view.hpp"
#include "view/video_profile.hpp"
#include <tinyxml2.h>

using namespace brls::literals;

PlayerView::PlayerView(const jellyfin::Item& item, const uint64_t seekTicks, const std::string& sourceId)
    : itemId(item.Id) {
    float width = brls::Application::contentWidth;
    float height = brls::Application::contentHeight;
    view = new VideoView();
    view->setDimensions(width, height);
    view->setWidthPercentage(100);
    view->setHeightPercentage(100);
    view->setId("video");
    this->setDimensions(width, height);
    this->addView(view);
    view->registerVideoQuality([this](...) { return this->toggleQuality(); });

    auto& mpv = MPVCore::instance();

    brls::Application::pushActivity(new brls::Activity(this), brls::TransitionAnimation::NONE);

    playSubscribeID = view->getPlayEvent()->subscribe([this](int index) { this->playIndex(index); });

    settingSubscribeID = view->getSettingEvent()->subscribe([this]() {
        brls::View* setting = new PlayerSetting(&this->stream);
        brls::Application::pushActivity(new brls::Activity(setting));
    });

    eventSubscribeID = mpv.getEvent()->subscribe([this](MpvEventEnum event) {
        auto& mpv = MPVCore::instance();
        // brls::Logger::info("mpv event => : {}", event);
        switch (event) {
        case MpvEventEnum::MPV_RESUME:
            this->tracker.onResume();
            this->reportPlay();
            view->getProfile()->init(this->playMethod);
            break;
        case MpvEventEnum::MPV_PAUSE:
            this->tracker.onPause();
            this->reportPlay(true);
            break;
        case MpvEventEnum::LOADING_START:
            this->tracker.onStallStart();
            break;
        case MpvEventEnum::LOADING_END:
            this->tracker.onFirstFrame();
            this->tracker.onStallEnd();
            this->reportStart();
            break;
        case MpvEventEnum::CACHE_SPEED_CHANGE:
            this->tracker.sampleSpeed(mpv.getCacheSpeedBps());
            break;
        case MpvEventEnum::MPV_STOP:
            this->tracker.finish("user_exit");
            this->reportStop();
            break;
        case MpvEventEnum::MPV_FILE_ERROR:
            this->tracker.finish("error");
            this->reportStop();
            break;
        case MpvEventEnum::END_OF_FILE:
            this->tracker.finish("eof");
            this->reportStop();
            break;
        case MpvEventEnum::MPV_LOADED: {
            auto& svr = AppConfig::instance().getUrl();
            const char* flag = MPVCore::SUBS_FALLBACK ? "select" : "auto";
            // 移除其他备用链接
            for (auto& s : this->stream.MediaStreams) {
                if (s.Type == jellyfin::streamTypeSubtitle) {
                    if (s.DeliveryUrl.size() > 0 && (s.IsExternal || this->playMethod == jellyfin::methodTranscode)) {
                        std::string url = svr + s.DeliveryUrl;
                        mpv.command("sub-add", url.c_str(), flag, s.DisplayTitle.c_str());
                    }
                }
            }
            if (PlayerSetting::selectedSubtitle > 0 && this->playMethod == jellyfin::methodDirectPlay) {
                mpv.setInt("sid", PlayerSetting::selectedSubtitle);
            }
            if (DanmakuCore::PLUGIN_ACTIVE && !this->stream.IsInfiniteStream) {
                this->requestDanmaku();
            }
            break;
        }
        case MpvEventEnum::UPDATE_PROGRESS:
            if (mpv.video_progress % 10 == 0) this->reportPlay();
            break;
        default:;
        }
    });
    // 自定义的mpv事件
    customEventSubscribeID = mpv.getCustomEvent()->subscribe([this](const std::string& event, void* data) {
        if (event == QUALITY_CHANGE) {
            this->playMedia(MPVCore::instance().playback_time * jellyfin::PLAYTICKS);
        } else if (event == SYNC_STOP) {
            VideoView::close();
        } else if (event == "PreviousTrack") {
            this->view->playNext(-1);
        } else if (event == "NextTrack") {
            this->view->playNext(1);
        }
    });

    if (item.Type != jellyfin::mediaTypeTvChannel) {
        this->sourceId = sourceId.empty() ? item.Id : sourceId;
    }

    this->setChapters(item.Chapters, item.RunTimeTicks);
    this->playMedia(seekTicks > 0 ? seekTicks : item.UserData.PlaybackPositionTicks);

    // Report stop when application exit
    this->exitSubscribeID = brls::Application::getExitEvent()->subscribe([this]() {
        if (!MPVCore::instance().isStopped()) this->reportStop();
    });
}

PlayerView::~PlayerView() {
    auto& mpv = MPVCore::instance();
    mpv.getEvent()->unsubscribe(eventSubscribeID);
    mpv.getCustomEvent()->unsubscribe(customEventSubscribeID);
    view->getPlayEvent()->unsubscribe(playSubscribeID);
    view->getSettingEvent()->unsubscribe(settingSubscribeID);

    brls::sync([&mpv]() { mpv.getCustomEvent()->fire(VIDEO_CLOSE, nullptr); });

    if (DanmakuCore::PLUGIN_ACTIVE) {
        DanmakuCore::instance().reset();
    }

    PlayerSetting::selectedSubtitle = 0;
    PlayerSetting::selectedAudio = 0;

    if (!mpv.isStopped()) this->reportStop();
    brls::Application::getExitEvent()->unsubscribe(this->exitSubscribeID);
    brls::Logger::debug("trying delete PlayerView...");
}

void PlayerView::setSeries(const std::string& seriesId) {
    std::string query = HTTP::encode_form({
        {"isVirtualUnaired", "false"},
        {"isMissing", "false"},
        {"userId", AppConfig::instance().getUserId()},
        {"fields", "Chapters"},
    });

    ASYNC_RETAIN
    jellyfin::getJSON<jellyfin::Result<jellyfin::Episode>>(
        [ASYNC_TOKEN](const jellyfin::Result<jellyfin::Episode>& r) {
            ASYNC_RELEASE
            int index = -1;
            std::vector<std::string> values;
            for (size_t i = 0; i < r.Items.size(); i++) {
                auto& item = r.Items.at(i);
                if (item.Id == this->itemId) index = i;
                values.push_back(fmt::format("S{}E{} - {}", item.ParentIndexNumber, item.IndexNumber, item.Name));
            }
            view->setList(values, index);
            this->episodes = std::move(r.Items);
        },
        [ASYNC_TOKEN](const std::string& error) {
            ASYNC_RELEASE
            Dialog::show(error);
        },
        jellyfin::apiShowEpisodes, seriesId, query);
}

void PlayerView::setTitie(const std::string& title) { this->view->setTitie(title); }

void PlayerView::setChapters(const std::vector<jellyfin::MediaChapter>& chaps, uint64_t duration) {
    this->view->setChapters(chaps, duration);
}

bool PlayerView::playIndex(int index) {
    if (index < 0 || index >= (int)this->episodes.size()) {
        return VideoView::close();
    }
    MPVCore::instance().reset();

    auto item = this->episodes.at(index);
    this->itemId = item.Id;
    this->sourceId = item.Id;
    this->setChapters(item.Chapters, item.RunTimeTicks);
    this->playMedia(0);
    view->setTitie(fmt::format("S{}E{} - {}", item.ParentIndexNumber, item.IndexNumber, item.Name));
    return true;
}

void PlayerView::playMedia(const uint64_t seekTicks) {
#if defined(__PS4__)
    int maxAllowedHeight = 1080;
#elif defined(__PSV__)
    int maxAllowedHeight = 720;
#else
    int maxAllowedHeight = brls::Application::windowHeight;
    if (MPVCore::VIDEO_QUALITY <= 0) {
    } else if (MPVCore::VIDEO_QUALITY <= 420000) {
        maxAllowedHeight = 360;
    } else if (MPVCore::VIDEO_QUALITY <= 720000) {
        maxAllowedHeight = 480;
    } else if (MPVCore::VIDEO_QUALITY <= 4000000) {
        maxAllowedHeight = 720;
    } else if (MPVCore::VIDEO_QUALITY <= 8000000) {
        maxAllowedHeight = 1080;
    } else if (MPVCore::VIDEO_QUALITY <= 15000000) {
        maxAllowedHeight = 1440;
    } else if (MPVCore::VIDEO_QUALITY <= 120000000) {
        maxAllowedHeight = 2160;
    }
#endif
    nlohmann::json conditions = {
#if defined(__PSV__)
        {
            {"Condition", "EqualsAny"},
            {"Property", "VideoProfile"},
            {"Value", "high|main|baseline"},
            {"IsRequired", false},
        },
        {
            {"Condition", "LessThanEqual"},
            {"Property", "VideoLevel"},
            {"Value", 40},
            {"IsRequired", false},
        },
#endif
        {
            {"Condition", "LessThanEqual"},
            {"Property", "Height"},
            {"Value", maxAllowedHeight},
            {"IsRequired", false},
        },
    };

    nlohmann::json profile = {
        {"MaxStreamingBitrate", (MPVCore::VIDEO_QUALITY <= 0) ? 120000000 : MPVCore::VIDEO_QUALITY},
        {
            "DirectPlayProfiles",
            {
                {
                    {"Type", "Audio"},
#if defined(__PSV__)
                    {"AudioCodec", "aac,mp3"},
#endif
                },
                {
                    {"Type", "Video"},
#ifdef __SWITCH__
                    {"VideoCodec", "h264,hevc,av1,vp9"},
#elif defined(__PSV__)
                    {"AudioCodec", "aac,mp3"},
                    {"VideoCodec", "h264"},
#endif
                },
            },
        },
#if defined(__PSV__)
        {
            "CodecProfiles",
            {
                {
                    {"Type", "Video"},
                    {"Codec", "h264"},
                    {"Conditions", conditions},
                },
            },
        },
#endif
        {
            "TranscodingProfiles",
            {
                {{"Type", "Audio"}},
                {
                    {"Container", "ts"},
                    {"Type", "Video"},
#if defined(__PSV__)
                    {"VideoCodec", "h264"},
                    {"AudioCodec", "aac,mp3"},
#else
                    {"VideoCodec", MPVCore::VIDEO_CODEC + ",mpeg4,mpeg2video"},
                    {"AudioCodec", "aac,mp3,ac3,opus,vorbis"},
#endif
                    {"Protocol", "hls"},
                    {"Conditions", conditions},
                },
            },
        },
        {
            "SubtitleProfiles",
            {
                {{"Format", "srt"}, {"Method", "External"}},
                {{"Format", "srt"}, {"Method", "Embed"}},
                {{"Format", "ass"}, {"Method", "External"}},
                {{"Format", "ass"}, {"Method", "Embed"}},
                {{"Format", "ssa"}, {"Method", "External"}},
                {{"Format", "ssa"}, {"Method", "Embed"}},
                {{"Format", "sub"}, {"Method", "External"}},
                {{"Format", "sub"}, {"Method", "Embed"}},
                {{"Format", "smi"}, {"Method", "External"}},
                {{"Format", "smi"}, {"Method", "Embed"}},
                {{"Format", "vtt"}, {"Method", "External"}},
                {{"Format", "dvdsub"}, {"Method", "Embed"}},
                {{"Format", "dvbsub"}, {"Method", "Embed"}},
                {{"Format", "pgssub"}, {"Method", "Embed"}},
                {{"Format", "pgs"}, {"Method", "Embed"}},
            },
        },
    };

    nlohmann::json param = {
        {"UserId", AppConfig::instance().getUserId()},
        {"MediaSourceId", this->sourceId},
#if defined(__PSV__)
        {"AlwaysBurnInSubtitleWhenTranscoding", PlayerSetting::selectedSubtitle > 0},
#endif
        {"AllowAudioStreamCopy", true},
        {"DeviceProfile", profile},
    };

    if (PlayerSetting::selectedAudio > 0) param.push_back({"AudioStreamIndex", PlayerSetting::selectedAudio});
    if (PlayerSetting::selectedSubtitle > 0) param.push_back({"SubtitleStreamIndex", PlayerSetting::selectedSubtitle});

    ASYNC_RETAIN
    jellyfin::postJSON(
        param,
        [ASYNC_TOKEN, seekTicks](const jellyfin::PlaybackResult& r) {
            ASYNC_RELEASE

            if (r.MediaSources.empty()) {
                Dialog::show(r.ErrorCode, []() { VideoView::close(); });
                return;
            }

            auto& mpv = MPVCore::instance();
            auto& svr = AppConfig::instance().getUrl();
            this->playSessionId = r.PlaySessionId;

            for (auto& item : r.MediaSources) {
                std::stringstream ssextra;
#ifdef _DEBUG
                for (auto& s : item.MediaStreams) {
                    brls::Logger::info("Track {} type {} => {}", s.Index, s.Type, s.DisplayTitle);
                }
#endif
                ssextra << fmt::format("network-timeout={}", HTTP::TIMEOUT / 100);
                if (seekTicks > 0) ssextra << ",start=" << misc::sec2Time(seekTicks / jellyfin::PLAYTICKS);
                if (item.IsInfiniteStream) view->hideVideoProgressSlider();

                auto startTracker = [this](const std::string& sessId) {
                    jellyfin::Stream vStream;
                    for (const auto& s : this->stream.MediaStreams) {
                        if (s.Type == jellyfin::streamTypeVideo) { vStream = s; break; }
                    }
                    this->tracker.start(sessId.empty() ? misc::randHex(16) : sessId,
                                        this->playMethod, vStream, this->stream.Bitrate);
                };

                if (item.IsRemote && MPVCore::FORCE_DIRECTPLAY) {
                    this->playMethod = jellyfin::methodDirectPlay;
                    mpv.setUrl(item.Path, ssextra.str());
                    this->stream = std::move(item);
                    startTracker(this->playSessionId);
                    return;
                }

                if (HTTP::PROXY_STATUS) ssextra << ",http-proxy=\"" << HTTP::PROXY << "\"";

                if (item.SupportsDirectPlay || MPVCore::FORCE_DIRECTPLAY) {
                    std::string url = fmt::format(fmt::runtime(jellyfin::apiStream), this->itemId,
                        HTTP::encode_form({
                            {"static", "true"},
                            {"mediaSourceId", item.Id},
                            {"playSessionId", r.PlaySessionId},
                            {"tag", item.ETag},
                        }));
                    this->playMethod = jellyfin::methodDirectPlay;
                    mpv.setUrl(svr + url, ssextra.str());
                    this->stream = std::move(item);
                    startTracker(r.PlaySessionId);
                    return;
                }

                if (item.SupportsTranscoding) {
                    this->playMethod = jellyfin::methodTranscode;
                    mpv.setUrl(svr + item.TranscodingUrl, ssextra.str());
                    this->stream = std::move(item);
                    startTracker(r.PlaySessionId);
                    return;
                }
            }

            VideoView::close();
        },
        [ASYNC_TOKEN](const std::string& ex) {
            ASYNC_RELEASE
            Dialog::show(ex, []() { VideoView::close(); });
        },
        jellyfin::apiPlayback, this->itemId);
}
