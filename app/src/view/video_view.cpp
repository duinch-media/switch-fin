#include "view/video_view.hpp"
#include "utils/config.hpp"
#include "utils/keybind.hpp"
#include "view/danmaku_core.hpp"
#include "view/danmaku_setting.hpp"
#include "view/mpv_core.hpp"
#include "view/svg_image.hpp"
#include "view/video_profile.hpp"
#include "view/video_progress_slider.hpp"

using namespace brls::literals;

#define CHECK_OSD(shake)                                                              \
    if (this->isOsdLock) {                                                            \
        if (this->isOsdShown) {                                                       \
            brls::Application::giveFocus(this->osdLockBox);                           \
            if (shake) this->osdLockBox->shakeHighlight(brls::FocusDirection::RIGHT); \
        } else {                                                                      \
            this->showOSD(true);                                                      \
        }                                                                             \
        return true;                                                                  \
    }

static int getSeekRange(int current) {
    current = abs(current);
    if (current < 60) return 5;
    if (current < 300) return 10;
    if (current < 600) return 20;
    if (current < 1200) return 60;
    return current / 15;
}

VideoView::VideoView() {
    this->inflateFromXMLRes("xml/view/video_view.xml");
    brls::Logger::debug("VideoView: created");
    this->setHideHighlightBorder(true);
    this->setHideHighlightBackground(true);
    this->setHideClickAnimation(true);

    auto& mpv = MPVCore::instance();
    mpv.reset();
    mpv.enableVO(true);

    if (MPVCore::OSD_TV_MODE) this->setTvMode(true);

    this->input = brls::Application::getPlatform()->getInputManager();

    this->registerAction(
        "hints/back"_i18n, brls::BUTTON_B,
        [this](brls::View* view) {
            if (isOsdLock) {
                this->toggleOSD();
                return true;
            }
            if (MPVCore::OSD_TV_MODE && this->isOsdShown) {
                this->toggleOSD();
                return true;
            }
            return close();
        },
        true);

    this->registerActions(
        "\uE08F", brls::BUTTON_LB, KeyBind::getRewind(),
        [this](brls::View* view) -> bool {
            CHECK_OSD(true);
            this->seekingRange -= getSeekRange(this->seekingRange);
            this->requestSeeking(seekingRange);
            return true;
        },
        false, true);

    this->registerActions(
        "\uE08E", brls::BUTTON_RB, KeyBind::getForward(),
        [this](brls::View* view) -> bool {
            CHECK_OSD(true);
            this->seekingRange += getSeekRange(this->seekingRange);
            this->requestSeeking(seekingRange);
            return true;
        },
        false, true);

    this->registerActions(
        "toggleOSD", brls::BUTTON_Y, KeyBind::getVideoOsd(),
        [this](brls::View* view) -> bool {
            if (!this->seekingRange) this->toggleOSD();
            return true;
        },
        true);

    this->btnSetting->registerClickAction([this](brls::View* view) {
        this->settingEvent.fire();
        return true;
    });
    this->btnSetting->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnSetting));
    this->registerActions(
        "main/player/setting"_i18n, brls::BUTTON_X, KeyBind::getSetting(),
        [this](brls::View* view) {
            CHECK_OSD(true);
            this->settingEvent.fire();
            return true;
        },
        true);

    this->registerActions(
        "volumeUp", brls::BUTTON_NAV_UP, KeyBind::getVolumeUp(),
        [this](brls::View* view) -> bool {
            CHECK_OSD(true);
            auto& state = brls::Application::getControllerState();
            if (state.buttons[brls::BUTTON_RT]) {
                this->requestVolume((int)MPVCore::instance().volume + 5, 400);
                return true;
            }
            return false;
        },
        true, true);

    this->registerActions(
        "volumeDown", brls::BUTTON_NAV_DOWN, KeyBind::getVolumeDown(),
        [this](brls::View* view) -> bool {
            CHECK_OSD(true);
            auto& state = brls::Application::getControllerState();
            if (state.buttons[brls::BUTTON_RT]) {
                this->requestVolume((int)MPVCore::instance().volume - 5, 400);
                return true;
            }
            return false;
        },
        true, true);

    this->btnVolume->registerClickAction([this](brls::View* view) { return this->toggleVolume(view); });
    this->btnVolume->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnVolume));

    this->btnDanmakuToggle->registerClickAction([this](...) { return this->toggleDanmaku(); });
    this->registerAction(KeyBind::getDanmaku(), [this](...) { return this->toggleDanmaku(); });
    this->btnDanmakuToggle->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnDanmakuToggle));

    this->btnDanmakuSetting->registerClickAction([](...) {
        auto setting = new DanmakuSetting();
        brls::Application::pushActivity(new brls::Activity(setting));
        return true;
    });
    this->btnDanmakuSetting->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnDanmakuSetting));

    this->registerMpvEvent();

    osdSlider->getProgressSetEvent().subscribe([this](float progress) {
        brls::Logger::verbose("Set progress: {}", progress);
        this->showOSD(true);
        MPVCore::instance().seekToProgress(progress);
    });

    osdSlider->getProgressEvent().subscribe([this](float progress) { this->showOSD(false); });

    this->registerTouchGestures();

    this->btnToggle->registerClickAction([](...) {
        MPVCore::instance().togglePlay();
        return true;
    });
    this->btnToggle->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnToggle));

    this->osdLockBox->registerClickAction([this](...) { return this->toggleOSDLock(); });
    this->osdLockBox->addGestureRecognizer(new brls::TapGestureRecognizer(this->osdLockBox));

    this->btnClose->registerClickAction([](...) {
        brls::sync([]() { close(); });
        return true;
    });
    this->btnClose->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnClose));

    this->btnBackward->registerClickAction([this](...) {
        this->playIndexEvent.fire(--this->playIndex);
        return true;
    });
    this->btnBackward->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnBackward));

    this->btnForward->registerClickAction([this](...) {
        this->playIndexEvent.fire(++this->playIndex);
        return true;
    });
    this->btnForward->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnForward));

    this->registerActions(
        "main/player/toggle"_i18n, brls::BUTTON_A, KeyBind::getVideoPause(), [this](brls::View* view) {
            CHECK_OSD(true);
            MPVCore::instance().togglePlay();
            if (MPVCore::OSD_ON_TOGGLE) this->showOSD(true);
            return true;
        });

    this->profile = new VideoProfile();
    this->addView(this->profile);
    this->registerActions(
        "profile", brls::BUTTON_BACK, KeyBind::getVideoProfile(),
        [this](brls::View* view) { return this->toggleProfile(); }, true);
    this->btnCast->registerClickAction([this](...) { return this->toggleProfile(); });
    this->btnCast->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnCast));

    this->btnVideoSpeed->registerClickAction([this](...) { return this->toggleSpeed(); });
    this->btnVideoSpeed->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnVideoSpeed));
    this->registerActions(
        "main/player/speed"_i18n, brls::BUTTON_LSB, KeyBind::getVideoSpeed(),
        [this](...) { return this->toggleSpeed(); }, true);
}

VideoView::~VideoView() {
    brls::Logger::debug("trying delete VideoView...");
    this->unRegisterMpvEvent();
    disableDimming(false);
    MPVCore::instance().stop();
}

void VideoView::draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style style, brls::FrameContext* ctx) {
    auto& mpv = MPVCore::instance();
    if (!mpv.isValid()) return;

    mpv.draw(this->getFrame(), this->getAlpha());

    if (MPVCore::BOTTOM_BAR) {
        NVGcontext* vg = brls::Application::getNVGContext();
        bottomBarColor.a = alpha;
        float progress = mpv.playback_time / mpv.duration;
        progress = progress > 1.0f ? 1.0f : progress;
        nvgFillColor(vg, bottomBarColor);
        nvgBeginPath(vg);
        nvgRect(vg, x, y + h - 2, w * progress, 2);
        nvgFill(vg);
    }

    if (enableDanmaku) DanmakuCore::instance().draw(vg, x, y, w, h, alpha);

    brls::Time current = brls::getCPUTimeUsec();
    if (current < this->osdLastShowTime) {
        if (!this->isOsdShown) this->isOsdShown = true;

        if (!isOsdLock) {
            osdTopBox->setVisibility(brls::Visibility::VISIBLE);
            osdBottomBox->setVisibility(brls::Visibility::VISIBLE);
            osdBottomBox->frame(ctx);
            osdTopBox->frame(ctx);
        }

        osdLockBox->setVisibility(brls::Visibility::VISIBLE);
        osdLockBox->frame(ctx);
    } else if (this->isOsdShown) {
        this->isOsdShown = false;
        if (isChildFocused()) brls::Application::giveFocus(this);
        osdTopBox->setVisibility(brls::Visibility::INVISIBLE);
        osdBottomBox->setVisibility(brls::Visibility::INVISIBLE);
        osdLockBox->setVisibility(brls::Visibility::INVISIBLE);
    }

    if (current > this->hintLastShowTime) {
        this->hintBox->setVisibility(brls::Visibility::GONE);
        this->hintLastShowTime = 0;
    }

    this->buttonProcessing();

    if (speedHintBox->getVisibility() == brls::Visibility::VISIBLE) {
        speedHintBox->frame(ctx);
        brls::Rect frame = speedHintLabel->getFrame();
        int ta1 = ((current >> 10) % 800) * 0.3;
        float tx = frame.getMinX() - 50;
        float ty = frame.getMinY() + 4.5;

        for (int i = 0; i < 3; i++) {
            int offx = tx + i * 15;
            int ta2 = (ta1 + i * 40) % 240;
            if (ta2 > 120) ta2 = 240 - ta2;

            nvgBeginPath(vg);
            nvgMoveTo(vg, offx, ty);
            nvgLineTo(vg, offx, ty + 12);
            nvgLineTo(vg, offx + 12, ty + 6);
            nvgFillColor(vg, a(nvgRGBA(255, 255, 255, ta2 + 80)));
            nvgClosePath(vg);
            nvgFill(vg);
        }
    }

    osdCenterBox->frame(ctx);
    osdInfoBox->frame(ctx);

    if (profile->getVisibility() == brls::Visibility::VISIBLE) {
        if (current - this->profileLastShowTime > 2000000) {
            profile->update();
            this->profileLastShowTime = current;
        }
        profile->frame(ctx);
    }
}

void VideoView::invalidate() { View::invalidate(); }
