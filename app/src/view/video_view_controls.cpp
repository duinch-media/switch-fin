#include "view/video_view.hpp"
#include "utils/gesture.hpp"
#include "utils/misc.hpp"
#include "view/mpv_core.hpp"
#include "view/svg_image.hpp"
#include "view/video_progress_slider.hpp"

const int VIDEO_SEEK_NODELAY = 0;
const int VIDEO_DOUBLE_TAP_SEEK_BACKWARD = -5;
const int VIDEO_DOUBLE_TAP_SEEK_FORWARD = 30;

using namespace brls::literals;

void VideoView::registerTouchGestures() {
    /// Touch gestures:
    /// - Single tap: toggle OSD
    /// - Double tap left (< 37.5% width): seek backward -5s
    /// - Double tap right (> 62.5% width): seek forward +30s
    /// - Double tap center: toggle play/pause
    /// - Long press: 2x speed
    /// - Horizontal pan: seek
    /// - Vertical pan (left/right): brightness / volume
    this->addGestureRecognizer(new OsdGestureRecognizer([this](OsdGestureStatus status) {
        auto& mpv = MPVCore::instance();
        if (status.osdGestureType == OsdGestureType::TAP) {
            this->toggleOSD();
            return;
        }
        if (!MPVCore::TOUCH_GESTURE) return;

        switch (status.osdGestureType) {
        case OsdGestureType::DOUBLE_TAP_END: {
            if (isOsdLock) {
                this->toggleOSD();
                break;
            }
            auto frame = this->getFrame();
            float relX =
                (frame.getWidth() > 0) ? (status.position.x - frame.getMinX()) / frame.getWidth() : 0.5f;
            if (relX < 0.375f) {
                this->seekingRange += VIDEO_DOUBLE_TAP_SEEK_BACKWARD;
                this->requestSeeking(this->seekingRange);
            } else if (relX > 0.625f) {
                this->seekingRange += VIDEO_DOUBLE_TAP_SEEK_FORWARD;
                this->requestSeeking(this->seekingRange);
            } else {
                mpv.togglePlay();
            }
            break;
        }
        case OsdGestureType::LONG_PRESS_START: {
            if (isOsdLock) break;
            float cur = MPVCore::VIDEO_SPEED == 100 ? 2.0f : MPVCore::VIDEO_SPEED * 0.01f;
            MPVCore::instance().setSpeed(cur);
            this->speedHintLabel->setText(fmt::format(fmt::runtime("main/player/speed_up"_i18n), cur));
            this->speedHintBox->setVisibility(brls::Visibility::VISIBLE);
            break;
        }
        case OsdGestureType::LONG_PRESS_CANCEL:
        case OsdGestureType::LONG_PRESS_END:
            if (isOsdLock) {
                this->toggleOSD();
                break;
            }
            mpv.setSpeed(1.0f);
            this->speedHintBox->setVisibility(brls::Visibility::GONE);
            break;
        case OsdGestureType::HORIZONTAL_PAN_START:
            if (isOsdLock) break;
            infoIcon->setImageFromSVGRes("icon/ico-seeking.svg");
            osdInfoBox->setVisibility(brls::Visibility::VISIBLE);
            break;
        case OsdGestureType::HORIZONTAL_PAN_UPDATE:
            if (isOsdLock) break;
            this->requestSeeking(fmin(120.0f, mpv.duration) * status.deltaX);
            break;
        case OsdGestureType::HORIZONTAL_PAN_CANCEL:
            if (isOsdLock) break;
            this->requestSeeking(0, VIDEO_SEEK_NODELAY);
            break;
        case OsdGestureType::HORIZONTAL_PAN_END:
            if (isOsdLock) {
                this->toggleOSD();
                break;
            }
            this->requestSeeking(fmin(120.0f, mpv.duration) * status.deltaX, VIDEO_SEEK_NODELAY);
            break;
        case OsdGestureType::LEFT_VERTICAL_PAN_START:
            if (isOsdLock) break;
            if (brls::Application::getPlatform()->canSetBacklightBrightness()) {
                this->brightnessInit = brls::Application::getPlatform()->getBacklightBrightness();
                infoIcon->setImageFromSVGRes("icon/ico-sun-fill.svg");
                osdInfoBox->setVisibility(brls::Visibility::VISIBLE);
                break;
            }
        case OsdGestureType::RIGHT_VERTICAL_PAN_START:
            if (isOsdLock) break;
            this->volumeInit = mpv.volume;
            infoIcon->setImageFromSVGRes("icon/ico-volume.svg");
            osdInfoBox->setVisibility(brls::Visibility::VISIBLE);
            break;
        case OsdGestureType::LEFT_VERTICAL_PAN_UPDATE:
            if (isOsdLock) break;
            if (brls::Application::getPlatform()->canSetBacklightBrightness()) {
                this->requestBrightness(this->brightnessInit + status.deltaY);
                break;
            }
        case OsdGestureType::RIGHT_VERTICAL_PAN_UPDATE:
            if (isOsdLock) break;
            this->requestVolume(this->volumeInit + status.deltaY * 100);
            break;
        case OsdGestureType::LEFT_VERTICAL_PAN_CANCEL:
        case OsdGestureType::LEFT_VERTICAL_PAN_END:
            if (isOsdLock) {
                this->toggleOSD();
                break;
            }
            if (brls::Application::getPlatform()->canSetBacklightBrightness()) {
                osdInfoBox->setVisibility(brls::Visibility::GONE);
                break;
            }
        case OsdGestureType::RIGHT_VERTICAL_PAN_CANCEL:
        case OsdGestureType::RIGHT_VERTICAL_PAN_END:
            if (isOsdLock) {
                this->toggleOSD();
                break;
            }
            osdInfoBox->setVisibility(brls::Visibility::GONE);
            break;
        default:
            break;
        }
    }));
}

void VideoView::requestSeeking(int seek, int delay) {
    auto& mpv = MPVCore::instance();
    if (mpv.duration <= 0) {
        this->seekingRange = 0;
        return;
    }
    double progress = (mpv.playback_time + seek) / mpv.duration;

    if (progress < 0) {
        progress = 0;
        seek = (int64_t)mpv.playback_time * -1;
    } else if (progress > 1) {
        progress = 1;
        seek = mpv.duration;
    }

    showOSD(false);
    if (osdInfoBox->getVisibility() != brls::Visibility::VISIBLE) {
        osdInfoBox->setVisibility(brls::Visibility::VISIBLE);
        infoIcon->setImageFromSVGRes("icon/ico-seeking.svg");
    }
    infoLabel->setText(fmt::format("{:+d} s", seek));
    osdSlider->setProgress(progress);
    leftStatusLabel->setText(misc::sec2Time(mpv.duration * progress));

    brls::cancelDelay(this->seekingIter);
    if (delay <= VIDEO_SEEK_NODELAY) {
        osdInfoBox->setVisibility(brls::Visibility::GONE);
        this->seekingRange = 0;
        this->showOSD(true);
        if (seek == 0) return;
        MPVCore::instance().seek(seek, "relative");
    } else if (delay > 0) {
        ASYNC_RETAIN
        this->seekingIter = brls::delay(delay, [ASYNC_TOKEN, seek]() {
            ASYNC_RELEASE
            osdInfoBox->setVisibility(brls::Visibility::GONE);
            this->seekingRange = 0;
            this->showOSD(true);
            if (seek == 0) return;
            MPVCore::instance().seek(seek, "relative");
        });
    }
}

void VideoView::requestVolume(int value, int delay) {
    if (value < 0) value = 0;
    if (value > 200) value = 200;
    MPVCore::instance().setInt("volume", value);
    infoLabel->setText(fmt::format("{:+d} %", value));

    if (delay == 0) return;
    if (this->volumeIter == 0) {
        osdInfoBox->setVisibility(brls::Visibility::VISIBLE);
        infoIcon->setImageFromSVGRes("icon/ico-volume.svg");
    } else {
        brls::cancelDelay(this->volumeIter);
    }
    ASYNC_RETAIN
    volumeIter = brls::delay(delay, [ASYNC_TOKEN]() {
        ASYNC_RELEASE
        osdInfoBox->setVisibility(brls::Visibility::GONE);
        this->volumeIter = 0;
    });
}

void VideoView::requestBrightness(float value) {
    if (value < 0) value = 0.0f;
    if (value > 1) value = 1.0f;
    brls::Application::getPlatform()->setBacklightBrightness(value);
    infoLabel->setText(fmt::format("{} %", (int)(value * 100)));
    infoIcon->setImageFromSVGRes("icon/ico-sun-fill.svg");
    osdInfoBox->setVisibility(brls::Visibility::VISIBLE);
}

void VideoView::registerMpvEvent() {
    auto& mpv = MPVCore::instance();
    this->eventSubscribeID = mpv.getEvent()->subscribe([this](MpvEventEnum event) {
        auto& mpv = MPVCore::instance();
        switch (event) {
        case MpvEventEnum::MPV_RESUME:
            if (MPVCore::OSD_ON_TOGGLE) this->showOSD(true);
            this->toggleIcon->setImageFromSVGRes("icon/ico-pause.svg");
            break;
        case MpvEventEnum::MPV_PAUSE:
            if (MPVCore::OSD_ON_TOGGLE) this->showOSD(false);
            hideLoading(false);
            this->toggleIcon->setImageFromSVGRes("icon/ico-play.svg");
            break;
        case MpvEventEnum::START_FILE:
            if (MPVCore::OSD_ON_TOGGLE) this->showOSD(false);
            break;
        case MpvEventEnum::LOADING_START:
            this->showLoading();
            break;
        case MpvEventEnum::LOADING_END:
            this->hideLoading();
            break;
        case MpvEventEnum::UPDATE_DURATION:
            if (this->seekingRange == 0) {
                this->rightStatusLabel->setText(misc::sec2Time(mpv.duration));
                this->osdSlider->setProgress((float)mpv.playback_time / mpv.duration);
            }
            break;
        case MpvEventEnum::UPDATE_PROGRESS:
            if (this->seekingRange == 0) {
                this->leftStatusLabel->setText(misc::sec2Time(mpv.video_progress));
                this->osdSlider->setProgress((float)mpv.playback_time / mpv.duration);
            }
            break;
        case MpvEventEnum::VIDEO_SPEED_CHANGE:
            if (std::fabs(mpv.video_speed - 1) < 1e-5) {
                this->videoSpeedLabel->setText("main/player/speed"_i18n);
            } else {
                this->videoSpeedLabel->setText(fmt::format("{}x", mpv.video_speed));
            }
            break;
        case MpvEventEnum::END_OF_FILE:
            disableDimming(false);
            this->toggleIcon->setImageFromSVGRes("icon/ico-play.svg");
            this->playIndexEvent.fire(++this->playIndex);
            break;
        case MpvEventEnum::CACHE_SPEED_CHANGE:
            if (this->osdCenterBox->getVisibility() != brls::Visibility::GONE) {
                if (this->centerLabel->getVisibility() != brls::Visibility::VISIBLE)
                    this->centerLabel->setVisibility(brls::Visibility::VISIBLE);
                this->centerLabel->setText(MPVCore::instance().getCacheSpeed());
            }
            break;
        case MpvEventEnum::VIDEO_MUTE:
            this->volumeIcon->setImageFromSVGRes("icon/ico-volume-off.svg");
            break;
        case MpvEventEnum::VIDEO_UNMUTE:
            this->volumeIcon->setImageFromSVGRes("icon/ico-volume.svg");
            break;
        case MpvEventEnum::MPV_FILE_ERROR: {
            auto dialog = new brls::Dialog("main/player/error"_i18n);
            dialog->addButton("hints/back"_i18n, []() { VideoView::close(true); });
            dialog->open();
            break;
        }
        default:;
        }
    });
}

void VideoView::unRegisterMpvEvent() {
    auto& mpv = MPVCore::instance();
    mpv.getEvent()->unsubscribe(eventSubscribeID);
}

void VideoView::registerActions(const std::string& hintText, const brls::ControllerButton button,
    const brls::BrlsKeyCombination key, const brls::ActionListener& actionListener, bool hidden, bool allowRepeating) {
    this->registerAction(hintText, button, actionListener, hidden, allowRepeating);
    this->registerAction(key, actionListener, allowRepeating);
}
