#include <borealis.hpp>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "utils/config.hpp"
#include "utils/download.hpp"
#include "utils/thread.hpp"
#include "api/analytics.hpp"

#include "view/svg_image.hpp"
#include "view/custom_button.hpp"
#include "view/context_menu.hpp"
#include "view/auto_tab_frame.hpp"
#include "view/recycling_grid.hpp"
#include "view/h_recycling.hpp"
#include "view/recyling_video.hpp"
#include "view/video_progress_slider.hpp"
#include "view/gallery_view.hpp"
#include "view/search_list.hpp"
#include "view/arr_search_list.hpp"
#include "view/video_view.hpp"
#include "view/selector_cell.hpp"
#include "view/button_close.hpp"
#include "view/text_box.hpp"
#include "view/icon_button.hpp"
#include "view/mpv_core.hpp"

#include "activity/main_activity.hpp"
#include "activity/server_list.hpp"
#include "activity/hint_activity.hpp"
#include "activity/loading_activity.hpp"
#include "tab/home_tab.hpp"
#include "tab/media_folder.hpp"
#include "tab/search_tab.hpp"
#include "tab/remote_tab.hpp"
#include "tab/remote_view.hpp"
#include "tab/setting_tab.hpp"
#include "tab/request_tab.hpp"
#include "tab/arr_setting.hpp"
#include "telemetry/telemetry_client.hpp"

#if defined(__SDL2__)
#include <SDL2/SDL_main.h>
#endif

using namespace brls::literals;  // for _i18n

int main(int argc, char* argv[]) {
    auto appStartAt = std::chrono::steady_clock::now();
    std::vector<std::string> items;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-d") == 0) {
            brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
        } else if (std::strcmp(argv[i], "-v") == 0) {
            brls::Application::enableDebuggingView(true);
        } else if (std::strcmp(argv[i], "-t") == 0) {
            MPVCore::DEBUG = true;
        } else if (std::strcmp(argv[i], "-o") == 0) {
            const char* path = (i + 1 < argc) ? argv[++i] : "switchfin.log";
            brls::Logger::setLogOutput(std::fopen(path, "w+"));
        } else if (std::strcmp(argv[i], "-version") == 0) {
            brls::Logger::info("{} {}", AppVersion::getDeviceName(), AppVersion::getCommit());
            return 0;
        } else {
            items.push_back(argv[i]);
        }
    }

    std::setlocale(LC_ALL, "C.UTF-8");
    // Load cookies and settings
    auto& conf = AppConfig::instance();
    if (!conf.init()) {
        return 0;
    }

    // Init the app and i18n
    if (!brls::Application::init()) {
        brls::Logger::error("Unable to init application");
        return EXIT_FAILURE;
    }

    conf.initThemes();
    DownloadManager::instance().init();
    telemetry::TelemetryClient::instance().init(conf.configDir());

    // Return directly to the desktop when closing the application (only for NX)
    brls::Application::getPlatform()->exitToHomeMode(true);

    brls::Application::createWindow(fmt::format("{} for {}", AppVersion::getPackageName(), AppVersion::getPlatform()));

    // Have the application register an action on every activity that will quit when you press BUTTON_START
    brls::Application::setGlobalQuit(false);

    // Register custom views (including tabs, which are views)
    brls::Application::registerXMLView("SVGImage", SVGImage::create);
    brls::Application::registerXMLView("IconButton", IconButton::create);
    brls::Application::registerXMLView("MenuItem", MenuItem::create);
    brls::Application::registerXMLView("CustomButton", CustomButton::create);
    brls::Application::registerXMLView("SelectorCell", SelectorCell::create);
    brls::Application::registerXMLView("TextBox", TextBox::create);
    brls::Application::registerXMLView("ButtonClose", ButtonClose::create);
    brls::Application::registerXMLView("AutoTabFrame", AutoTabFrame::create);
    brls::Application::registerXMLView("RecyclingGrid", RecyclingGrid::create);
    brls::Application::registerXMLView("HRecyclerFrame", HRecyclerFrame::create);
    brls::Application::registerXMLView("RecylingVideo", RecylingVideo::create);
    brls::Application::registerXMLView("GalleryView", GalleryView::create);
    brls::Application::registerXMLView("SearchList", SearchList::create);
    brls::Application::registerXMLView("ArrSearchList", ArrSearchList::create);
    brls::Application::registerXMLView("VideoProgressSlider", VideoProgressSlider::create);

    brls::Application::registerXMLView("HomeTab", HomeTab::create);
    brls::Application::registerXMLView("MediaFolders", MediaFolders::create);
    brls::Application::registerXMLView("SearchTab", SearchTab::create);
    brls::Application::registerXMLView("RemoteTab", RemoteTab::create);
    brls::Application::registerXMLView("SettingTab", SettingTab::create);
    brls::Application::registerXMLView("RequestTab", RequestTab::create);
    brls::Application::registerXMLView("ArrSetting", ArrSetting::create);

    auto recordAppStartMetrics = [appStartAt]() {
        int64_t coldStartMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - appStartAt).count();
        bool appletMode = !brls::Application::getPlatform()->isApplicationMode();
        bool docked = false;
        int wifi = -1;
#ifdef __SWITCH__
        docked = (appletGetOperationMode() != AppletOperationMode_Handheld);
        NifmInternetConnectionType type;
        u32 strength = 0;
        NifmInternetConnectionStatus status;
        if (R_SUCCEEDED(nifmGetInternetConnectionStatus(&type, &strength, &status)) &&
            type == NifmInternetConnectionType_WiFi && strength <= 3) {
            wifi = static_cast<int>(strength);
        }
#endif
        telemetry::TelemetryClient::instance().recordAppStart(coldStartMs, appletMode, docked, wifi);
    };

    if (!brls::Application::getPlatform()->isApplicationMode()) {
        brls::Application::pushActivity(new HintActivity());
        recordAppStartMetrics();
    } else if (items.size() > 0) {
        RemoteView::play(items.front());
        recordAppStartMetrics();
    } else {
        brls::Application::pushActivity(new LoadingActivity(), brls::TransitionAnimation::NONE);
        brls::Application::blockInputs();
        brls::async([recordAppStartMetrics]() {
            const bool logged = AppConfig::instance().checkLogin();
            brls::sync([logged, recordAppStartMetrics]() {
                brls::Application::unblockInputs();
                brls::Application::clear();
                if (!logged) {
                    brls::Application::pushActivity(new ServerList());
                } else {
                    brls::Application::pushActivity(new MainActivity());
                }
                recordAppStartMetrics();
            });
        });
    }

    GA("open_app")

    AppVersion::checkUpdate();

    // Run the app
    while (brls::Application::mainLoop());

    int64_t uptimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - appStartAt).count();
    bool dockedAtExit = false;
#ifdef __SWITCH__
    dockedAtExit = (appletGetOperationMode() != AppletOperationMode_Handheld);
#endif
    telemetry::TelemetryClient::instance().recordAppExit(uptimeMs, dockedAtExit);

    // Submit queued telemetry before sockets and workers go away (short deadline, never blocks exit long).
    telemetry::TelemetryClient::instance().shutdown();
    ThreadPool::instance().stop();

    conf.checkRestart(argv);
    // Exit
    return EXIT_SUCCESS;
}
