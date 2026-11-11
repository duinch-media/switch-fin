#include "view/video_view.hpp"
#include "utils/config.hpp"
#include "utils/keybind.hpp"
#include "view/danmaku_core.hpp"
#include "view/mpv_core.hpp"
#include "view/svg_image.hpp"
#include "view/video_profile.hpp"
#include "view/video_progress_slider.hpp"

using namespace brls::literals;

void VideoView::onChildFocusGained(View* directChild, View* focusedView) {
    Box::onChildFocusGained(directChild, focusedView);
    if (isOsdLock) {
        brls::Application::giveFocus(this->osdLockBox);
        return;
    }
    if (this->isOsdShown) {
        if (focusedView->getParent()->getVisibility() == brls::Visibility::GONE) {
            brls::Application::giveFocus(this);
        }
        if (focusedView == this->osdSettingIcon) {
            this->osdSettingIcon->setCustomNavigationRoute(brls::FocusDirection::DOWN,
                lastFocusedView == this->btnToggle ? "video/osd/toggle" : "video/osd/lock/box");
        }
        lastFocusedView = focusedView;
        return;
    }
    brls::Application::giveFocus(this);
}

void VideoView::buttonProcessing() {
    auto& state = brls::Application::getControllerState();
    if (this->isOsdShown) {
        if (state.buttons[brls::BUTTON_NAV_RIGHT] || state.buttons[brls::BUTTON_NAV_LEFT] ||
            state.buttons[brls::BUTTON_NAV_UP] || state.buttons[brls::BUTTON_NAV_DOWN]) {
            if (this->osdState == OSDState::SHOWN) this->showOSD(true);
        }
    }
}

void VideoView::setTitie(const std::string& title) { this->titleLabel->setText(title); }

void VideoView::setList(const std::vector<std::string>& values, int index) {
    this->btnEpisode->registerClickAction([this, values](...) {
        brls::Dropdown* dropdown = new brls::Dropdown(
            "main/player/episode"_i18n, values,
            [this](int selected) {
                this->showLoading();
                this->playIndexEvent.fire(selected);
            },
            this->playIndex);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });
    this->btnEpisode->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnEpisode));
    this->btnEpisode->setVisibility(brls::Visibility::VISIBLE);
    this->showEpisodeLabel->setVisibility(brls::Visibility::VISIBLE);

    auto event = [this, values](int index) {
        this->playIndex = index;
        auto v1 = index > 0 ? brls::Visibility::VISIBLE : brls::Visibility::GONE;
        auto v2 = index + 1 < (int)values.size() ? brls::Visibility::VISIBLE : brls::Visibility::GONE;
        this->btnBackward->setVisibility(v1);
        for (brls::View* view : this->btnBackward->getChildren()) view->setVisibility(v1);
        this->btnForward->setVisibility(v2);
        for (brls::View* view : this->btnForward->getChildren()) view->setVisibility(v2);
    };
    this->playIndexEvent.subscribe(event);
    event(index);
}

void VideoView::showLoading() {
    this->centerLabel->setVisibility(brls::Visibility::INVISIBLE);
    this->osdCenterBox->setVisibility(brls::Visibility::VISIBLE);
    disableDimming(false);
}

void VideoView::hideLoading(bool dimming) {
    this->osdCenterBox->setVisibility(brls::Visibility::GONE);
    disableDimming(dimming);
}

bool VideoView::toggleProfile() {
    if (profile->getVisibility() == brls::Visibility::VISIBLE) {
        profile->setVisibility(brls::Visibility::INVISIBLE);
        return false;
    }
    profile->setVisibility(brls::Visibility::VISIBLE);
    profile->update();
    return true;
}

void VideoView::toggleOSD() {
    if (this->isOsdShown) this->hideOSD();
    else this->showOSD(true);
}

void VideoView::showOSD(bool autoHide) {
    if (autoHide) {
        this->osdLastShowTime = brls::getCPUTimeUsec() + VideoView::OSD_SHOW_TIME;
        this->osdState = OSDState::SHOWN;
    } else {
        this->osdLastShowTime = std::numeric_limits<std::time_t>::max();
        this->osdState = OSDState::ALWAYS_ON;
    }
}

void VideoView::hideOSD() {
    this->osdLastShowTime = 0;
    this->osdState = OSDState::HIDDEN;
}

void VideoView::showHint(const std::string& value) {
    brls::Logger::debug("Video hint: {}", value);
    this->hintLabel->setText(value);
    this->hintBox->setVisibility(brls::Visibility::VISIBLE);
    this->hintLastShowTime = brls::getCPUTimeUsec() + VideoView::OSD_SHOW_TIME;
    this->showOSD();
}

void VideoView::setTvMode(bool state) {
    btnToggle->setCustomNavigationRoute(brls::FocusDirection::RIGHT, state ? osdSlider : iconBox);
    volumeIcon->setCustomNavigationRoute(brls::FocusDirection::UP, state ? osdSlider : osdLockBox);
    danmakuSettingIcon->setCustomNavigationRoute(brls::FocusDirection::UP, state ? osdSlider : osdLockBox);
    danmakuIcon->setCustomNavigationRoute(brls::FocusDirection::UP, state ? osdSlider : osdLockBox);
    iconVideoQuality->setCustomNavigationRoute(brls::FocusDirection::UP, state ? osdSlider : osdLockBox);
    iconVideoSpeed->setCustomNavigationRoute(brls::FocusDirection::UP, state ? osdSlider : osdLockBox);
    osdLockBox->setCustomNavigationRoute(brls::FocusDirection::DOWN, state ? osdSlider : iconBox);
    osdSlider->setFocusable(state);
}

bool VideoView::toggleOSDLock() {
    this->isOsdLock = !this->isOsdLock;
    if (this->isOsdLock) {
        this->osdLockIcon->setImageFromSVGRes("icon/player-lock.svg");
        osdTopBox->setVisibility(brls::Visibility::GONE);
        osdBottomBox->setVisibility(brls::Visibility::GONE);
        osdLockBox->setCustomNavigationRoute(brls::FocusDirection::UP, "video/osd/lock/box");
        osdLockBox->setCustomNavigationRoute(brls::FocusDirection::DOWN, "video/osd/lock/box");
    } else {
        this->osdLockIcon->setImageFromSVGRes("icon/player-unlock.svg");
        osdLockBox->setCustomNavigationRoute(brls::FocusDirection::UP, "video/osd/setting");
        osdLockBox->setCustomNavigationRoute(brls::FocusDirection::DOWN, "video/speed/box");
    }
    return true;
}

bool VideoView::toggleSpeed() {
    brls::Dropdown* dropdown = new brls::Dropdown(
        "main/player/speed"_i18n, {"2.0x", "1.75x", "1.5x", "1.25x", "1.0x", "0.75x", "0.5x"},
        [](int selected) { MPVCore::instance().setSpeed((200 - selected * 25) / 100.0f); },
        int(200 - MPVCore::instance().video_speed * 100) / 25);
    brls::Application::pushActivity(new brls::Activity(dropdown));
    return true;
}

bool VideoView::toggleVolume(brls::View* view) {
    this->showOSD(false);
    auto theme = brls::Application::getTheme();
    auto container = new brls::Box();
    container->setHideClickAnimation(true);
    container->registerAction("hints/back"_i18n, brls::BUTTON_B, [this](brls::View* view) {
        this->showOSD(true);
        view->dismiss();
        return true;
    });
    container->addGestureRecognizer(new brls::TapGestureRecognizer(container, [this, container]() {
        this->showOSD(true);
        container->dismiss();
        return true;
    }));
    auto sliderBox = new brls::Box();
    sliderBox->setAlignItems(brls::AlignItems::CENTER);
    sliderBox->setHeight(40);
    sliderBox->setCornerRadius(4);
    sliderBox->setBackgroundColor(theme.getColor("color/grey_1"));
    float sliderX = view->getX() - 120;
    if (sliderX < 0) sliderX = 20;
    if (sliderX > brls::Application::ORIGINAL_WINDOW_WIDTH - 332)
        sliderX = brls::Application::ORIGINAL_WINDOW_WIDTH - 332;
    sliderBox->setTranslationX(sliderX);
    sliderBox->setTranslationY(view->getY() - 70);
    auto slider = new brls::Slider();
    slider->setMargins(8, 16, 8, 16);
    slider->setWidth(300);
    slider->setHeight(20);
    slider->setProgress(MPVCore::instance().getInt("volume") / 200.0f);
    slider->getProgressEvent()->subscribe([](float progress) { MPVCore::instance().setInt("volume", progress * 200); });
    sliderBox->addView(slider);
    container->addView(sliderBox);
    auto frame = new brls::AppletFrame(container);
    frame->setInFadeAnimation(true);
    frame->setHeaderVisibility(brls::Visibility::GONE);
    frame->setFooterVisibility(brls::Visibility::GONE);
    frame->setBackgroundColor(theme.getColor("brls/backdrop"));
    brls::Application::pushActivity(new brls::Activity(frame));
    return true;
}

bool VideoView::close(bool quit) {
    if (brls::Application::getActivitiesStack().size() > 1) {
        return brls::Application::popActivity(brls::TransitionAnimation::NONE);
    }
    if (quit) {
        brls::Application::quit();
        return true;
    }
    auto dialog = new brls::Dialog("hints/exit_hint"_i18n);
    dialog->addButton("hints/cancel"_i18n, []() {});
    dialog->addButton("hints/ok"_i18n, []() { brls::Application::quit(); });
    dialog->open();
    return false;
}

void VideoView::disableDimming(bool disable) {
    brls::Application::getPlatform()->disableScreenDimming(disable, "Playing video", AppVersion::getPackageName());
    brls::Application::setAutomaticDeactivation(!disable);
}

void VideoView::setDanmakuEnable(brls::Visibility v) {
    this->enableDanmaku = (v == brls::Visibility::VISIBLE);
    btnDanmakuToggle->setVisibility(v);
    danmakuIcon->setVisibility(v);
    if (enableDanmaku) {
        this->refreshDanmakuIcon();
    } else {
        btnDanmakuSetting->setVisibility(v);
        danmakuSettingIcon->setVisibility(v);
    }
}

bool VideoView::toggleDanmaku() {
    if (!enableDanmaku) return false;
    DanmakuCore::DANMAKU_ON = !DanmakuCore::DANMAKU_ON;
    this->refreshDanmakuIcon();
    AppConfig::instance().setItem(AppConfig::DANMAKU_ON, DanmakuCore::DANMAKU_ON);
    return true;
}

void VideoView::refreshDanmakuIcon() {
    if (DanmakuCore::DANMAKU_ON) {
        this->danmakuIcon->setImageFromSVGRes("icon/ico-danmu-switch-on.svg");
        btnDanmakuSetting->setVisibility(brls::Visibility::VISIBLE);
        danmakuSettingIcon->setVisibility(brls::Visibility::VISIBLE);
    } else {
        this->danmakuIcon->setImageFromSVGRes("icon/ico-danmu-switch-off.svg");
        if (brls::Application::getCurrentFocus() == btnDanmakuSetting) {
            brls::Application::giveFocus(btnDanmakuToggle);
        }
        btnDanmakuSetting->setVisibility(brls::Visibility::GONE);
        danmakuSettingIcon->setVisibility(brls::Visibility::GONE);
    }
}

void VideoView::setChapters(const std::vector<jellyfin::MediaChapter>& chaps, uint64_t duration) {
    if (chaps.empty()) {
        this->osdSlider->clearClipPoint();
        return;
    }
    if (MPVCore::CLIP_POINT) {
        std::vector<float> clips;
        for (auto& c : chaps) clips.push_back(float(c.StartPositionTicks) / float(duration));
        this->osdSlider->setClipPoint(clips);
    }
    this->btnVideoChapter->registerClickAction([chaps](brls::View* view) {
        int selectedChapter = -1;
        uint64_t ticks = MPVCore::instance().video_progress * jellyfin::PLAYTICKS;
        std::vector<std::string> values;
        for (auto& item : chaps) {
            values.push_back(item.Name);
            if (item.StartPositionTicks <= ticks) selectedChapter++;
        }
        brls::Dropdown* dropdown = new brls::Dropdown(
            "main/player/chapter"_i18n, values,
            [chaps](int selected) {
                int64_t offset = chaps[selected].StartPositionTicks;
                MPVCore::instance().seek(offset / jellyfin::PLAYTICKS);
            },
            selectedChapter);
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });
    this->btnVideoChapter->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnVideoChapter));
    this->btnVideoChapter->setVisibility(brls::Visibility::VISIBLE);
    for (auto& view : this->btnVideoChapter->getChildren()) view->setVisibility(brls::Visibility::VISIBLE);
}

void VideoView::playNext(int offset) { this->playIndexEvent.fire(this->playIndex + offset); }
void VideoView::hideVideoProgressSlider() { this->osdSlider->setVisibility(brls::Visibility::GONE); }

void VideoView::hideVideoQuality() {
    this->btnVideoQuality->setVisibility(brls::Visibility::GONE);
    for (brls::View* view : this->btnVideoQuality->getChildren()) view->setVisibility(brls::Visibility::GONE);
}

void VideoView::registerVideoQuality(brls::ActionListener action) {
    this->btnVideoQuality->registerClickAction(action);
    this->btnVideoQuality->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnVideoQuality));
    this->registerActions("main/player/quality"_i18n, brls::BUTTON_RSB, KeyBind::getVideoQuality(), action, true);
}
