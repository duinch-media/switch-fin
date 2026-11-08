#ifdef __SWITCH__
#include <switch.h>
#elif defined(__PSV__)
#include <psp2/appmgr.h>
#include <psp2/vshbridge.h>
#include <borealis/platforms/desktop/desktop_platform.hpp>
#elif defined(ANDROID)
#include <SDL2/SDL_system.h>
#include <jni.h>
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
#include <unistd.h>
#include <borealis/platforms/desktop/desktop_platform.hpp>
#if defined(_WIN32)
#include <shlobj.h>
#endif
#endif

#include <borealis.hpp>
#include "utils/config.hpp"
#include "utils/misc.hpp"

std::unordered_map<AppConfig::Item, AppConfig::Option> AppConfig::settingMap = {
    {APP_THEME, {"app_theme", {"auto", "light", "dark"}}},
    {APP_LANG, {"app_lang", {brls::LOCALE_AUTO, brls::LOCALE_EN_US, brls::LOCALE_ZH_HANS, brls::LOCALE_ZH_HANT,
                                brls::LOCALE_JA, brls::LOCALE_Ko, brls::LOCALE_RU, brls::LOCALE_DE, brls::LOCALE_FR,
                                brls::LOCALE_ES, brls::LOCALE_PT, "cs", "uk", "tr", "vi"}}},
    {APP_UI_SCALE, {"app_ui_scale", {"544p", "720p", "900p", "1080p"}}},
    {AUDIO_CHANNELS, {"audio-channels", {"auto-safe", "stereo", "mono"}}},
    {KEYMAP, {"keymap", {"xbox", "ps", "keyboard"}}},
    {WINDOW_STATE, {"window_state"}},
    {TRANSCODEC, {"transcodec", {"h264", "hevc", "av1"}}},
    {FORCE_DIRECTPLAY, {"force_directplay"}},
    {MAXDAY_NEXTUP, {"maxday_nextup"}},
    {FULLSCREEN, {"fullscreen"}},
    {OSD_ON_TOGGLE, {"osd_on_toggle"}},
    {TOUCH_GESTURE, {"touch_gesture"}},
    {CLIP_POINT, {"clip_point"}},
    {SYNC_SETTING, {"sync_setting"}},
    {OVERCLOCK, {"overclock"}},
    {MPV_VO, {"mpv_vo", {"gpu", "gpu-next", "mediacodec_embed"}}},
    {MPV_RENDER, {"mpv_render"}},
    {PLAYER_BOTTOM_BAR, {"player_bottom_bar"}},
    {PLAYER_LOW_QUALITY, {"player_low_quality"}},
    {PLAYER_SUBS_FALLBACK, {"player_subs_fallback"}},
    {PLAYER_INMEMORY_CACHE,
        {
            "player_inmemory_cache",
            {"0MB", "10MB", "20MB", "50MB", "100MB", "200MB", "500MB"},
            {0, 10, 20, 50, 100, 200, 500},
        }},
    {PLAYER_SPEED,
        {
            "player_speed",
            {"4x", "3x", "2x"},
            {400, 300, 200},
        }},
    {PLAYER_HWDEC, {"player_hwdec"}},
    {PLAYER_HWDEC_CUSTOM, {"player_hwdec_custom"}},
    {PLAYER_ASPECT, {"player_aspect", {"auto", "stretch", "crop", "4:3", "16:9"}}},
    {PLAYER_TV_MODE, {"player_tv_mode"}},
    {DANMAKU, {"danmaku"}},
    {DANMAKU_ON, {"danmaku_on"}},
    {DANMAKU_STYLE_AREA, {"danmaku_style_area", {"1/4", "1/2", "3/4", "1"}, {25, 50, 75, 100}}},
    {DANMAKU_STYLE_ALPHA,
        {
            "danmaku_style_alpha",
            {"10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"},
            {10, 20, 30, 40, 50, 60, 70, 80, 90, 100},
        }},
    {DANMAKU_STYLE_FONTSIZE,
        {"danmaku_style_fontsize", {"50%", "75%", "100%", "125%", "150%", "175%"}, {15, 22, 30, 37, 45, 50}}},
    {DANMAKU_STYLE_FONT, {"danmaku_style_font", {"stroke", "incline", "shadow", "pure"}}},
    {DANMAKU_STYLE_LINE_HEIGHT,
        {
            "danmaku_style_line_height",
            {"100%", "120%", "140%", "160%", "180%", "200%"},
            {100, 120, 140, 160, 180, 200},
        }},
    {DANMAKU_STYLE_SPEED,
        {
            "danmaku_style_speed",
            {"0.5", "0.75", "1.0", "1.25", "1.5"},
            {150, 125, 100, 75, 50},
        }},
    {DANMAKU_RENDER_QUALITY,
        {
            "danmaku_render_quality",
            {"100%", "95%", "90%", "80%", "70%", "60%", "50%"},
            {100, 95, 90, 80, 70, 60, 50},
        }},
    {ALWAYS_ON_TOP, {"always_on_top"}},
    {SINGLE, {"single"}},
    {SHOW_FPS, {"show_fps"}},
    {SWAP_INTERVAL, {"swap_interval"}},
    {APP_SWAP_ABXY, {"app_swap_abxy"}},
    {TEXTURE_CACHE_NUM, {"texture_cache_num"}},
    {REQUEST_THREADS, {"request_threads", {"1", "2", "4", "8"}, {1, 2, 4, 8}}},
    {REQUEST_TIMEOUT,
        {"request_timeout", {"3000", "5000", "10000", "20000", "30000"}, {3000, 5000, 10000, 20000, 30000}}},
    {HTTP_PROXY_STATUS, {"http_proxy_status"}},
    {HTTP_PROXY, {"http_proxy"}},
    {DOWNLOAD_QUALITY, {"download_quality", {"Original", "1080p", "720p", "480p"}, {0, 1, 2, 3}}},
    {DOWNLOAD_PATH, {"download_path"}},
    {KEY_REFRESH, {"key_refresh"}},
    {KEY_LAST, {"key_last"}},
    {KEY_NEXT, {"key_next"}},
    {KEY_VOLUME_UP, {"key_volume_up"}},
    {KEY_VOLUME_DOWN, {"key_volume_down"}},
    {KEY_VIDEO_PROFILE, {"key_video_profile"}},
    {KEY_DANMAKU, {"key_danmaku"}},
    {KEY_FORWARD, {"key_forward"}},
    {KEY_REWIND, {"key_rewind"}},
    {KEY_SETTING, {"key_setting"}},
    {KEY_VIDEO_QUALITY, {"key_video_quality"}},
    {KEY_VIDEO_SPEED, {"key_video_speed"}},
    {KEY_VIDEO_OSD, {"key_video_osd"}},
    {KEY_VIDEO_PAUSE, {"key_video_pause"}},
};

std::string generateDeviceId() {
#ifdef __SWITCH__
    AccountUid uid;
    accountInitialize(AccountServiceType_Administrator);
    if (R_FAILED(accountGetPreselectedUser(&uid))) {
        if (R_FAILED(accountTrySelectUserWithoutInteraction(&uid, false))) {
            accountGetLastOpenedUser(&uid);
        }
    }
    accountExit();
    if (accountUidIsValid(&uid)) {
        uint8_t digest[32];
        sha256CalculateHash(digest, &uid, sizeof(uid));
        return misc::hexEncode(digest, sizeof(digest));
    }
#elif defined(__PSV__)
    char cid[0x20];
    if (_vshSblAimgrGetConsoleId(cid) >= 0) {
        char text[0x40];
        sceClibSnprintf(text, sizeof(text) - 1, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
            cid[0x0], cid[0x1], cid[0x2], cid[0x3], cid[0x4], cid[0x5], cid[0x6], cid[0x7], cid[0x8], cid[0x9],
            cid[0xA], cid[0xB], cid[0xC], cid[0xD], cid[0xE], cid[0xF]);
        return text;
    }
#elif defined(ANDROID)
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jclass utilsClass = env->FindClass("org/libsdl/app/PlatformUtils");
    if (utilsClass) {
        jmethodID jmethod = env->GetStaticMethodID(utilsClass, "getAndroidId", "()Ljava/lang/String;");
        jstring jname = (jstring)env->CallStaticObjectMethod(utilsClass, jmethod);
        const char* name = env->GetStringUTFChars(jname, nullptr);
        std::string deviceId = name;
        env->ReleaseStringUTFChars(jname, name);
        env->DeleteLocalRef(jname);
        env->DeleteLocalRef(utilsClass);
        return deviceId;
    }
#elif defined(_WIN32)
    HW_PROFILE_INFOW profile;
    if (GetCurrentHwProfileW(&profile)) {
        std::vector<char> deviceId(HW_PROFILE_GUIDLEN);
        WideCharToMultiByte(CP_UTF8, 0, profile.szHwProfileGuid, std::wcslen(profile.szHwProfileGuid), deviceId.data(),
            deviceId.size(), nullptr, nullptr);
        return deviceId.data();
    }
#elif defined(__APPLE__)
    io_registry_entry_t ioRegistryRoot = IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/");
    if (ioRegistryRoot) {
        CFStringRef uuidCf = (CFStringRef)IORegistryEntryCreateCFProperty(
            ioRegistryRoot, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
        std::vector<char> deviceId(CFStringGetLength(uuidCf) + 1);
        CFStringGetCString(uuidCf, deviceId.data(), deviceId.size(), kCFStringEncodingMacRoman);
        CFRelease(uuidCf);
        IOObjectRelease(ioRegistryRoot);
        return deviceId.data();
    }
#elif defined(__linux__)
    static const std::vector<std::string> dev_names = {
        "/sys/devices/virtual/dmi/id/board_serial",
        "/proc/device-tree/serial-number",
        "/etc/machine-id",
    };
    for (auto& path : dev_names) {
        std::ifstream f(path.c_str());
        if (f.is_open()) {
            std::string name;
            std::getline(f, name);
            if (name.size() > 0) {
                return misc::hexEncode((uint8_t*)name.data(), name.size());
            }
        }
    }
#endif
    return misc::randHex(16);
}

std::string AppConfig::configDir() const {
#if __SWITCH__
    return fmt::format("sdmc:/switch/{}", AppVersion::getPackageName());
#elif defined(__PS4__)
    return fmt::format("/data/{}", AppVersion::getPackageName());
#elif defined(__PSV__)
    return fmt::format("ux0:/data/{}", AppVersion::getPackageName());
#elif _WIN32
    WCHAR wpath[MAX_PATH];
    std::vector<char> lpath(MAX_PATH);
    SHGetSpecialFolderPathW(0, wpath, CSIDL_LOCAL_APPDATA, false);
    WideCharToMultiByte(CP_UTF8, 0, wpath, std::wcslen(wpath), lpath.data(), lpath.size(), nullptr, nullptr);
    return fmt::format("{}\\{}", lpath.data(), AppVersion::getPackageName());
#elif defined(ANDROID)
    return SDL_AndroidGetExternalStoragePath();
#elif __linux__
    char* config_home = getenv("XDG_CONFIG_HOME");
    if (config_home) return fmt::format("{}/{}", config_home, AppVersion::getPackageName());
    return fmt::format("{}/.config/{}", getenv("HOME"), AppVersion::getPackageName());
#elif __APPLE__
    return fmt::format("{}/Library/Application Support/{}", getenv("HOME"), AppVersion::getPackageName());
#endif
}

std::string AppConfig::downloadDir() const {
    const std::string fallback = this->configDir() + "/downloads";
    std::string dir = this->getItem<std::string>(AppConfig::DOWNLOAD_PATH, fallback);
    if (dir.empty()) dir = fallback;

    try {
        auto path = fs::u8path(dir);
        if (!fs::exists(path)) fs::create_directories(path);
        return dir;
    } catch (const std::exception& e) {
        brls::Logger::warning("Invalid download dir '{}': {}", dir, e.what());
        return fallback;
    }
}

std::string AppConfig::ipcSocket() const {
#ifdef _WIN32
    return "\\\\.\\pipe\\" + AppVersion::getPackageName();
#else
    return fmt::format("{}/{}.sock", configDir(), AppVersion::getPackageName());
#endif
}

void AppConfig::checkRestart(char* argv[]) {
#if !defined(__PS4__) && !defined(__SWITCH__) && !defined(ANDROID)
    if (brls::DesktopPlatform::RESTART_APP) {
        brls::Logger::info("Restart app {}", argv[0]);
#if defined(__PSV__)
        sceAppMgrLoadExec(argv[0], argv, nullptr);
#else
        execv(argv[0], argv);
#endif
    }
#endif
}

int AppConfig::getOptionIndex(const Item item, int default_index) const {
    auto it = settingMap.find(item);
    if (it == settingMap.end()) return default_index;
    if (setting.contains(it->second.key)) {
        try {
            std::string value = this->setting.at(it->second.key);
            for (size_t i = 0; i < it->second.options.size(); ++i)
                if (it->second.options[i] == value) return i;
        } catch (const std::exception& e) {
            brls::Logger::error("Damaged config found: {}/{}", it->second.key, e.what());
        }
    }
    return default_index;
}

int AppConfig::getValueIndex(const Item item, int default_index) const {
    auto it = settingMap.find(item);
    if (it == settingMap.end()) return default_index;
    if (setting.contains(it->second.key)) {
        try {
            long value = this->setting.at(it->second.key);
            for (size_t i = 0; i < it->second.values.size(); ++i)
                if (it->second.values[i] == value) return i;
        } catch (const std::exception& e) {
            brls::Logger::error("Damaged config found: {}/{}", it->second.key, e.what());
        }
    }
    return default_index;
}
