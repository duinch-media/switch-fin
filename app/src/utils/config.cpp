#ifdef __SWITCH__
#include <switch.h>
#include "utils/overclock.hpp"
#elif defined(__PSV__)
#include <psp2/kernel/cpu.h>
#include <psp2/kernel/threadmgr/thread.h>
#include <psp2/vshbridge.h>
#include <borealis/platforms/desktop/desktop_platform.hpp>

extern "C" {
unsigned int _newlib_heap_size_user = 220 * 1024 * 1024;
unsigned int _pthread_stack_default_user = 2 * 1024 * 1024;
}
#elif defined(__PS4__)
#include <orbis/SystemService.h>
#include <orbis/Sysmodule.h>
#include <arpa/inet.h>

extern "C" {
extern int ps4_mpv_use_precompiled_shaders;
extern int ps4_mpv_dump_shaders;
extern in_addr_t primary_dns;
extern in_addr_t secondary_dns;
}
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
#include <unistd.h>
#include <borealis/platforms/desktop/desktop_platform.hpp>

constexpr uint32_t MINIMUM_WINDOW_WIDTH = 640;
constexpr uint32_t MINIMUM_WINDOW_HEIGHT = 360;
#endif

#include <borealis.hpp>
#include <borealis/core/cache_helper.hpp>
#include "api/http.hpp"
#include "utils/config.hpp"
#include "utils/keybind.hpp"
#include "utils/misc.hpp"
#include "utils/ums.hpp"
#include "utils/thread.hpp"
#include "view/mpv_core.hpp"
#include "view/danmaku_core.hpp"
#include <lunasvg.h>

extern std::string generateDeviceId();

static int defaultPlatformInMemoryCacheMb() {
#if defined(__PSV__) || defined(TRIMUI)
    return 20;
#elif defined(__SWITCH__) || defined(__PS4__) || defined(ANDROID)
    return 100;
#else
    return 200;
#endif
}

bool AppConfig::init() {
    const std::string path = this->configDir() + "/config.json";
#if !defined(USE_BOOST_FILESYSTEM) || defined(_WIN32)
    std::ifstream f(fs::u8path(path));
#else
    std::ifstream f(path);
#endif
    if (f.is_open()) {
        try {
            nlohmann::json::parse(f).get_to(*this);
            brls::Logger::info("Load config from: {}", path);
        } catch (const std::exception& ex) {
            brls::Logger::error("AppConfig::load: {}", ex.what());
            return false;
        }
    }

#if defined(_WIN32) && !defined(_WINRT_)
    misc::initCrashDump();
#endif

#if (defined(__APPLE__) || defined(__linux__) || defined(_WIN32)) && !defined(ANDROID) && !defined(TRIMUI)
    brls::DesktopPlatform::GAMEPAD_DB = configDir() + "/gamecontrollerdb.txt";
    if (this->getItem(AppConfig::SINGLE, false) && misc::sendIPC(this->ipcSocket(), "{}")) {
        brls::Logger::warning("AppConfig single instance");
        return false;
    }
    auto wstate = this->getItem(AppConfig::WINDOW_STATE, std::string{""});
    if (wstate.size() > 0) {
        int hXPos, hYPos, monitor;
        uint32_t hWidth, hHeight;
        sscanf(wstate.c_str(), "%d,%ux%u,%dx%d", &monitor, &hWidth, &hHeight, &hXPos, &hYPos);
        if (hWidth > 0 && hHeight > 0) {
            VideoContext::sizeH = hHeight;
            VideoContext::sizeW = hWidth;
            VideoContext::posX = (float)hXPos;
            VideoContext::posY = (float)hYPos;
            VideoContext::monitorIndex = monitor;
        }
    }
    brls::Application::getExitEvent()->subscribe([this]() {
        if (std::isnan(VideoContext::posX) || std::isnan(VideoContext::posY) || VideoContext::FULLSCREEN) return;
        auto videoContext = brls::Application::getPlatform()->getVideoContext();
        uint32_t width = VideoContext::sizeW ? VideoContext::sizeW : brls::Application::ORIGINAL_WINDOW_WIDTH;
        uint32_t height = VideoContext::sizeH ? VideoContext::sizeH : brls::Application::ORIGINAL_WINDOW_HEIGHT;
        this->setItem(AppConfig::WINDOW_STATE, fmt::format("{},{}x{},{}x{}", videoContext->getCurrentMonitorIndex(),
                                                   width, height, (int)VideoContext::posX, (int)VideoContext::posY));
        this->save();
    });
#elif defined(__PSV__)
    int search_unk[2];
    if (_vshKernelSearchModuleByName("CapUnlocker", search_unk) >= 0) {
        brls::sync([]() { brls::Application::notify("CapUnlocker found"); });
        sceKernelChangeThreadPriority(SCE_KERNEL_THREAD_ID_SELF, 64);
        sceKernelChangeThreadCpuAffinityMask(SCE_KERNEL_THREAD_ID_SELF, SCE_KERNEL_CPU_MASK_SYSTEM);
    }
#elif defined(__PS4__)
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0) brls::Logger::error("cannot load net module");
    primary_dns = inet_addr("223.5.5.5");
    secondary_dns = inet_addr("1.1.1.1");
    ps4_mpv_use_precompiled_shaders = 1;
    ps4_mpv_dump_shaders = 0;
    brls::sync([]() { sceSystemServiceHideSplashScreen(); });
#endif

    std::string uiScale = this->getItem(APP_UI_SCALE, std::string(""));
    if (uiScale == "544p") {
        brls::Application::ORIGINAL_WINDOW_WIDTH = 960;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = 544;
    } else if (uiScale == "720p") {
        brls::Application::ORIGINAL_WINDOW_WIDTH = 1280;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = 720;
    } else if (uiScale == "900p") {
        brls::Application::ORIGINAL_WINDOW_WIDTH = 1600;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = 900;
    } else if (uiScale == "1080p") {
        brls::Application::ORIGINAL_WINDOW_WIDTH = 1920;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = 1080;
    }

    AppConfig::SYNC = this->getItem(SYNC_SETTING, true);
    HTTP::TIMEOUT = this->getItem(REQUEST_TIMEOUT, 3000L);
    HTTP::PROXY_STATUS = this->getItem(HTTP_PROXY_STATUS, false);
    HTTP::PROXY = this->getItem(HTTP_PROXY, std::string("http://192.168.1.1:1080"));
    VideoContext::FULLSCREEN = this->getItem(FULLSCREEN, false);

    MPVCore::BOTTOM_BAR = this->getItem(PLAYER_BOTTOM_BAR, true);
    MPVCore::OSD_ON_TOGGLE = this->getItem(OSD_ON_TOGGLE, true);
    MPVCore::TOUCH_GESTURE = this->getItem(TOUCH_GESTURE, true);
    MPVCore::CLIP_POINT = this->getItem(CLIP_POINT, true);

    const int defaultCacheMb = defaultPlatformInMemoryCacheMb();
    if (!this->setting.contains("player_inmemory_cache")) {
        this->setting["player_inmemory_cache"] = defaultCacheMb;
    }
    MPVCore::INMEMORY_CACHE = this->getItem(PLAYER_INMEMORY_CACHE, defaultCacheMb);

#if defined(__PSV__) || defined(__PS4__) || defined(__SWITCH__)
    MPVCore::LOW_QUALITY = this->getItem(PLAYER_LOW_QUALITY, true);
#else
    MPVCore::LOW_QUALITY = this->getItem(PLAYER_LOW_QUALITY, false);
#endif
    MPVCore::SUBS_FALLBACK = this->getItem(PLAYER_SUBS_FALLBACK, true);
    MPVCore::VO = this->getItem(MPV_VO, MPVCore::VO);
    MPVCore::HARDWARE_DEC = this->getItem(PLAYER_HWDEC, true);
    MPVCore::FORCE_DIRECTPLAY = this->getItem(FORCE_DIRECTPLAY, false);
    MPVCore::VIDEO_CODEC = this->getItem(TRANSCODEC, MPVCore::VIDEO_CODEC);
    MPVCore::AUDIO_CHANNELS = this->getItem(AUDIO_CHANNELS, MPVCore::AUDIO_CHANNELS);
    MPVCore::PLAYER_HWDEC_METHOD = this->getItem(PLAYER_HWDEC_CUSTOM, MPVCore::PLAYER_HWDEC_METHOD);
    MPVCore::VIDEO_SPEED = this->getItem(PLAYER_SPEED, MPVCore::VIDEO_SPEED);
    MPVCore::VIDEO_ASPECT = this->getItem(PLAYER_ASPECT, MPVCore::VIDEO_ASPECT);
    MPVCore::OSD_TV_MODE = this->getItem(PLAYER_TV_MODE, false);

    DanmakuCore::DANMAKU_ON = this->getItem(DANMAKU_ON, true);
    DanmakuCore::DANMAKU_STYLE_AREA = this->getItem(DANMAKU_STYLE_AREA, 100);
    DanmakuCore::DANMAKU_STYLE_ALPHA = this->getItem(DANMAKU_STYLE_ALPHA, 80);
    DanmakuCore::DANMAKU_STYLE_FONTSIZE = this->getItem(DANMAKU_STYLE_FONTSIZE, 30);
    DanmakuCore::DANMAKU_STYLE_LINE_HEIGHT = this->getItem(DANMAKU_STYLE_LINE_HEIGHT, 120);
    DanmakuCore::DANMAKU_STYLE_SPEED = this->getItem(DANMAKU_STYLE_SPEED, 100);

    ThreadPool::max_thread_num = this->getItem(REQUEST_THREADS, ThreadPool::max_thread_num);
    if (this->device.empty()) this->device = generateDeviceId();

    brls::Platform::APP_LOCALE_DEFAULT = this->getItem(APP_LANG, brls::LOCALE_AUTO);
    brls::Application::setFPSStatus(this->getItem(SHOW_FPS, false));
    VideoContext::swapInterval = this->getItem(SWAP_INTERVAL, 1);

    KeyBind::setLast(this->getItem(KEY_LAST, std::string{"pgup"}));
    KeyBind::setNext(this->getItem(KEY_NEXT, std::string{"pgdn"}));
    KeyBind::setVolumeUp(this->getItem(KEY_VOLUME_UP, std::string{"0"}));
    KeyBind::setVolumeDown(this->getItem(KEY_VOLUME_DOWN, std::string{"9"}));
    KeyBind::setDanmaku(this->getItem(KEY_DANMAKU, std::string{"d"}));
    KeyBind::setVideoProfile(this->getItem(KEY_VIDEO_PROFILE, std::string{"f1"}));
    KeyBind::setVideoQuality(this->getItem(KEY_VIDEO_QUALITY, std::string{"f2"}));
    KeyBind::setVideoSpeed(this->getItem(KEY_VIDEO_SPEED, std::string{"f3"}));
    KeyBind::setSetting(this->getItem(KEY_SETTING, std::string{"f4"}));
    KeyBind::setRefresh(this->getItem(KEY_REFRESH, std::string{"f5"}));
    KeyBind::setForward(this->getItem(KEY_FORWARD, std::string{"]"}));
    KeyBind::setRewind(this->getItem(KEY_REWIND, std::string{"["}));
    KeyBind::setVideoOsd(this->getItem(KEY_VIDEO_OSD, std::string{"o"}));
    KeyBind::setVideoPause(this->getItem(KEY_VIDEO_PAUSE, std::string{"space"}));

    brls::Application::getWindowCreationDoneEvent()->subscribe([this]() {
#if defined(TRIMUI)
        if (this->getItem(APP_SWAP_ABXY, true))
#else
        if (this->getItem(APP_SWAP_ABXY, false))
#endif
            brls::Application::setSwapInputKeys(!brls::Application::isSwapInputKeys());

        std::string danmakuFont = this->configDir() + "/danmaku.ttf";
        if (brls::Application::getPlatform()->isApplicationMode() && access(danmakuFont.c_str(), F_OK) != -1 &&
            brls::Application::loadFontFromFile("danmaku", danmakuFont)) {
            int danmakuFontId = brls::Application::getFont("danmaku");
            nvgAddFallbackFontId(
                brls::Application::getNVGContext(), danmakuFontId, brls::Application::getDefaultFont());
            DanmakuCore::DANMAKU_FONT = danmakuFontId;
        } else {
            DanmakuCore::DANMAKU_FONT = brls::Application::getDefaultFont();
        }
        std::string appTheme = this->getItem(APP_THEME, std::string("auto"));
        if (appTheme == "light")
            brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::LIGHT);
        else if (appTheme == "dark")
            brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);

#if defined(__PSV__) || defined(__PS4__)
        brls::TextureCache::instance().cache.setCapacity(1);
#else
        brls::TextureCache::instance().cache.setCapacity(getItem(TEXTURE_CACHE_NUM, 200));
#endif

#if (defined(__APPLE__) || defined(__linux__) || defined(_WIN32)) && !defined(ANDROID)
        brls::Application::getPlatform()->setWindowSizeLimits(MINIMUM_WINDOW_WIDTH, MINIMUM_WINDOW_HEIGHT, 0, 0);
        if (this->getItem(ALWAYS_ON_TOP, false)) brls::Application::getPlatform()->setWindowAlwaysOnTop(true);
#endif

        brls::Application::getPlatform()->getInputManager()->getKeyboardKeyStateChanged()->subscribe(
            [this](brls::KeyState state) {
                if (!state.pressed) return;
#ifndef __APPLE__
                if (state.key == brls::BRLS_KBD_KEY_F11) {
                    VideoContext::FULLSCREEN = !this->getItem(AppConfig::FULLSCREEN, VideoContext::FULLSCREEN);
                    this->setItem(AppConfig::FULLSCREEN, VideoContext::FULLSCREEN);
                    brls::Application::getPlatform()->getVideoContext()->fullScreen(VideoContext::FULLSCREEN);
                }
#endif
            });
    });

#ifdef __SWITCH__
    if (getItem(AppConfig::OVERCLOCK, false)) SwitchSys::setClock(true);
    PlFontData font;
    if (R_SUCCEEDED(plGetSharedFontByType(&font, PlSharedFontType_Standard)))
        lunasvg_add_font_face_from_data("", false, false, font.address, font.size, nullptr, nullptr);
#elif !defined(USE_LIBROMFS)
    lunasvg_add_font_face_from_file("", false, false, BRLS_ASSET("font/switch_font.ttf"));
#else
    auto& font = romfs::get("font/switch_font.ttf");
    if (font.valid()) lunasvg_add_font_face_from_data("", false, false, font.data(), font.size(), nullptr, nullptr);
#endif
    Ums::instance().init();

    brls::FontLoader::USER_FONT_PATH = configDir() + "/font.ttf";
    brls::FontLoader::USER_ICON_PATH = configDir() + "/icon.ttf";
    if (access(brls::FontLoader::USER_ICON_PATH.c_str(), F_OK) == -1) {
#if defined(__PSV__) || defined(__PS4__)
        brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_ps.ttf");
#else
        std::string icon = getItem(KEYMAP, std::string("xbox"));
        if (icon == "xbox")
            brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_xbox.ttf");
        else if (icon == "ps")
            brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_ps.ttf");
        else if (brls::Application::isSwapInputKeys())
            brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_keyboard_swap.ttf");
        else
            brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_keyboard.ttf");
#endif
    }

    brls::FontLoader::USER_EMOJI_PATH = configDir() + "/emoji.ttf";
    if (access(brls::FontLoader::USER_EMOJI_PATH.c_str(), F_OK) == -1)
        brls::FontLoader::USER_EMOJI_PATH = BRLS_ASSET("font/emoji.ttf");

    brls::Logger::info("init {} v{}-{} device {} from {}", AppVersion::getPlatform(), AppVersion::getVersion(),
        AppVersion::getCommit(), this->device, path);
    return true;
}
