#include "view/mpv_core.hpp"
#include "api/http.hpp"
#include "utils/config.hpp"
#include "utils/misc.hpp"
#include <fmt/ranges.h>

extern void configureMpvPlatformBuffering(mpv_handle *mpv);

static inline void check_error(int status) {
    if (status < 0) brls::Logger::error("MPV ERROR => {}", mpv_error_string(status));
}

#ifndef MPV_SW_RENDER
#ifdef BOREALIS_USE_D3D11
#include <borealis/platforms/driver/d3d11.hpp>
extern std::unique_ptr<brls::D3D11Context> D3D11_CONTEXT;
#elif defined(BOREALIS_USE_DEKO3D)
#include <borealis/platforms/switch/switch_video.hpp>
#elif defined(BOREALIS_USE_GXM)
#include <borealis/platforms/psv/psv_video.hpp>
#include <borealis/extern/nanovg/nanovg_gxm.h>
#else
#ifdef __SDL2__
#include <SDL2/SDL.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#if defined(__PS4__) || defined(__PSV__) || defined(__SWITCH__) || defined(ANDROID)
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>
#endif
#endif
static void *get_proc_address(void *unused, const char *name) {
#ifdef __SDL2__
    SDL_GL_GetCurrentContext();
    return (void *)SDL_GL_GetProcAddress(name);
#else
    glfwGetCurrentContext();
    return (void *)glfwGetProcAddress(name);
#endif
}
#endif
#endif

#ifdef ANDROID
#include <jni.h>
extern "C" {
#include <libavcodec/jni.h>
}
static JavaVM *g_vm;
static jobject surface;

static int64_t getNativeSurface() {
    int64_t ptr = 0;
    JNIEnv *env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    jclass cls = env->GetObjectClass(activity);
    jmethodID jmethod = env->GetStaticMethodID(cls, "getMpvSurface", "()Landroid/view/Surface;");
    jobject surface_ = env->CallStaticObjectMethod(cls, jmethod);
    if (surface_ != nullptr) {
        surface = env->NewGlobalRef(surface_);
        ptr = (int64_t)(intptr_t)surface;
        env->DeleteLocalRef(surface_);
    }
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return ptr;
}

static void deleteSurfaceObj() {
    auto env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
    env->DeleteGlobalRef(surface);
}
#endif

void MPVCore::on_wakeup(void *self) {
    MPVCore *mpv = reinterpret_cast<MPVCore *>(self);
    brls::sync([mpv]() { mpv->eventMainLoop(); });
}

MPVCore::MPVCore() {
    this->init();
    brls::Application::getExitEvent()->subscribe([this]() {
        this->clean();
#ifdef MPV_SW_RENDER
        if (this->pixels) {
            free(this->pixels);
            this->pixels = nullptr;
            this->mpv_params[3].data = nullptr;
        }
#endif
    });
}

void MPVCore::init() {
    std::setlocale(LC_NUMERIC, "C");
#ifdef ANDROID
    auto env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
    if (!env->GetJavaVM(&g_vm) && g_vm) av_jni_set_java_vm(g_vm, NULL);
#endif
    this->mpv = mpv_create();
    if (!mpv) brls::fatal("Error Create mpv Handle");

    auto &conf = AppConfig::instance();
    std::string confDir = conf.configDir();

    mpv_set_option_string(mpv, "config", "yes");
    mpv_set_option_string(mpv, "config-dir", confDir.c_str());
    mpv_set_option_string(mpv, "sub-fonts-dir", confDir.c_str());
    mpv_set_option_string(mpv, "watch-later-dir", fmt::format("{}/watch-later", confDir).c_str());
    mpv_set_option_string(mpv, "gpu-shader-cache-dir", fmt::format("{}/cache", confDir).c_str());
    mpv_set_option_string(mpv, "ytdl", "no");
    mpv_set_option_string(mpv, "referrer", conf.getUrl().c_str());
    mpv_set_option_string(mpv, "user-agent", HTTP::USER_AGENT.c_str());
    mpv_set_option_string(mpv, "osd-level", "0");
    mpv_set_option_string(mpv, "video-timing-offset", "0");
    mpv_set_option_string(mpv, "reset-on-next-file", "speed,pause");
    mpv_set_option_string(mpv, "subs-fallback", SUBS_FALLBACK ? "yes" : "no");
    mpv_set_option_string(mpv, "vo", MPVCore::VO.c_str());
#if defined(__PS4__) || defined(__PSV__) || defined(TRIMUI)
    mpv_set_option_string(mpv, "audio-channels", "stereo");
#else
    mpv_set_option_string(mpv, "audio-channels", MPVCore::AUDIO_CHANNELS.c_str());
#endif

    if (MPVCore::LOW_QUALITY) {
        brls::Logger::info("lavc: skip loop filter and set fast decode");
        mpv_set_option_string(mpv, "vd-lavc-skiploopfilter", "all");
        mpv_set_option_string(mpv, "vd-lavc-fast", "yes");
        if (mpv_client_api_version() > MPV_MAKE_VERSION(2, 1)) mpv_set_option_string(mpv, "profile", "fast");
    }

    configureMpvPlatformBuffering(mpv);

#if defined(__SWITCH__)
    mpv_set_option_string(mpv, "vd-lavc-dr", "yes");
    mpv_set_option_string(mpv, "vd-lavc-threads", "4");
    mpv_set_option_string(mpv, "opengl-glfinish", "yes");
    std::string locale = brls::Application::getPlatform()->getLocale();
    if (locale == brls::LOCALE_ZH_HANS)
        mpv_set_option_string(mpv, "sub-font", "nintendo_udsg-r_org_zh-cn_003");
    else if (locale == brls::LOCALE_ZH_HANT)
        mpv_set_option_string(mpv, "sub-font", "nintendo_udjxh-db_zh-tw_003");
    else if (locale == brls::LOCALE_Ko)
        mpv_set_option_string(mpv, "sub-font", "nintendo_udsg-r_ko_003");
    else {
        mpv_set_option_string(mpv, "sub-font", "subfont.ttf");
        mpv_set_option_string(mpv, "sub-font-provider", "none");
        mpv_set_option_string(mpv, "sub-ass-override", "force");
    }
#elif defined(__PS4__)
    mpv_set_option_string(mpv, "vd-lavc-threads", "6");
#elif defined(__PSV__)
    mpv_set_option_string(mpv, "vd-lavc-threads", "4");
    mpv_set_option_string(mpv, "fbo-format", "rgba8");
    mpv_set_option_string(mpv, "video-latency-hacks", "yes");
#elif defined(ANDROID)
    mpv_set_option_string(mpv, "gpu-context", "android");
    mpv_set_option_string(mpv, "opengl-es", "yes");
#endif

    if (MPVCore::HARDWARE_DEC) {
        mpv_set_option_string(mpv, "hwdec", PLAYER_HWDEC_METHOD.c_str());
        brls::Logger::info("MPV hardware decode: {}", PLAYER_HWDEC_METHOD);
    } else {
        mpv_set_option_string(mpv, "hwdec", "no");
    }

    if (MPVCore::DEBUG) {
        mpv_set_option_string(mpv, "terminal", "yes");
        mpv_set_option_string(mpv, "msg-level", "all=v");
    } else if (brls::Application::isDebuggingViewEnabled()) {
        mpv_request_log_messages(mpv, "info");
    }

#if (defined(__APPLE__) || defined(__linux__) || defined(_WIN32)) && !defined(ANDROID)
    if (conf.getItem(AppConfig::SINGLE, false))
        mpv_set_option_string(mpv, "input-ipc-server", conf.ipcSocket().c_str());
#endif

    if (mpv_initialize(mpv) < 0) {
        mpv_terminate_destroy(mpv);
        brls::fatal("Could not initialize mpv context");
    }

    this->setAspect(VIDEO_ASPECT);

    check_error(mpv_observe_property(mpv, 1, "core-idle", MPV_FORMAT_FLAG));
    check_error(mpv_observe_property(mpv, 2, "pause", MPV_FORMAT_FLAG));
    check_error(mpv_observe_property(mpv, 3, "duration", MPV_FORMAT_INT64));
    check_error(mpv_observe_property(mpv, 4, "playback-time", MPV_FORMAT_DOUBLE));
    check_error(mpv_observe_property(mpv, 5, "cache-speed", MPV_FORMAT_INT64));
    check_error(mpv_observe_property(mpv, 9, "speed", MPV_FORMAT_DOUBLE));
    check_error(mpv_observe_property(mpv, 10, "volume", MPV_FORMAT_INT64));

#ifdef ANDROID
    int64_t wid = getNativeSurface();
    mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, (void *)&wid);
    mpv_set_option_string(mpv, "force-window", "yes");
#elif defined(MPV_SW_RENDER)
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_SW)},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
#elif defined(BOREALIS_USE_D3D11)
    mpv_d3d11_init_params init_params{.device = D3D11_CONTEXT->getDevice()};
    const char *backend = conf.getItem(AppConfig::MPV_RENDER, false) ? "gpu-next" : "gpu";
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_D3D11)},
        {MPV_RENDER_PARAM_D3D11_INIT_PARAMS, &init_params},
        {MPV_RENDER_PARAM_BACKEND, const_cast<char *>(backend)},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
#elif defined(BOREALIS_USE_DEKO3D)
    auto videoContext = dynamic_cast<brls::SwitchVideoContext *>(brls::Application::getPlatform()->getVideoContext());
    mpv_deko3d_init_params deko_init_params{videoContext->getDeko3dDevice()};
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_DEKO3D)},
        {MPV_RENDER_PARAM_DEKO3D_INIT_PARAMS, &deko_init_params},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
#elif defined(BOREALIS_USE_GXM)
    auto videoContext = dynamic_cast<brls::PsvVideoContext *>(brls::Application::getPlatform()->getVideoContext());
    NVGXMwindow *gxm = videoContext->getWindow();
    NVGcontext *vg = brls::Application::getNVGContext();
    mpv_gxm_init_params gxm_params = {
        .context = gxm->context,
        .shader_patcher = gxm->shader_patcher,
        .buffer_index = 0,
        .msaa = SCE_GXM_MULTISAMPLE_4X,
    };
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_GXM)},
        {MPV_RENDER_PARAM_GXM_INIT_PARAMS, &gxm_params},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    if (!mpv_fbo.render_target) {
        int texture_width = DISPLAY_WIDTH;
        int texture_height = DISPLAY_HEIGHT;
        int texture_stride = ALIGN(texture_width, 8);
        nvg_image = nvgCreateImageRGBA(vg, texture_width, texture_height, 0, nullptr);
        NVGXMtexture *texture = nvgxmImageHandle(vg, nvg_image);

        NVGXMframebufferInitOptions framebufferOpts = {
            .display_buffer_count = 1,
            .scenesPerFrame = 1,
            .render_target = texture,
            .color_format = SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
            .color_surface_type = SCE_GXM_COLOR_SURFACE_LINEAR,
            .display_width = texture_width,
            .display_height = texture_height,
            .display_stride = texture_stride,
        };
        NVGXMframebuffer *fbo = gxmCreateFramebuffer(&framebufferOpts);
        mpv_fbo.render_target = fbo->gxm_render_target;
        mpv_fbo.color_surface = &fbo->gxm_color_surfaces[0].surface;
        mpv_fbo.depth_stencil_surface = &fbo->gxm_depth_stencil_surface;
        mpv_fbo.w = texture_width;
        mpv_fbo.h = texture_height;
    }
#else
    mpv_opengl_init_params gl_init_params{get_proc_address, nullptr};
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
#if defined(GLFW_EXPOSE_NATIVE_X11)
        {MPV_RENDER_PARAM_X11_DISPLAY, glfwGetX11Display()},
#endif
#if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
        {MPV_RENDER_PARAM_WL_DISPLAY, glfwGetWaylandDisplay()},
#endif
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
#endif

#ifndef ANDROID
    if (mpv_render_context_create(&mpv_context, mpv, params) < 0) {
        mpv_terminate_destroy(mpv);
        brls::fatal("failed to initialize mpv context");
    }
#endif
#ifdef BOREALIS_USE_D3D11
    misc::initCrashDump();
#endif
    brls::Logger::info("version: {} ffmpeg {}", mpv_get_property_string(mpv, "mpv-version"),
        mpv_get_property_string(mpv, "ffmpeg-version"));

    this->command("set", "audio-client-name", AppVersion::getPackageName().c_str());
    mpv_set_wakeup_callback(mpv, on_wakeup, this);
#ifndef ANDROID
    mpv_render_context_set_update_callback(mpv_context, on_update, this);
#endif

    focusSubscription = brls::Application::getWindowFocusChangedEvent()->subscribe([this](bool focus) {
        static bool playing = false;
        if (!focus) {
            playing = !isPaused();
            command("set", "pause", "yes");
        } else if (playing) {
            command("set", "pause", "no");
        }
#if defined(ANDROID)
        this->enableVO(focus);
#endif
    });

    sizeSubscription = brls::Application::getWindowSizeChangedEvent()->subscribe([this]() {
        setFrameSize(this->rect);
    });

#if defined(BOREALIS_USE_OPENGL) && !defined(MPV_SW_RENDER)
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &default_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, default_framebuffer);
    mpv_fbo.fbo = default_framebuffer;
#endif
}

void MPVCore::clean() {
    check_error(mpv_command_string(this->mpv, "quit"));
    brls::Application::getWindowFocusChangedEvent()->unsubscribe(focusSubscription);
    brls::Application::getWindowSizeChangedEvent()->unsubscribe(sizeSubscription);
    if (this->mpv_context) {
        mpv_render_context_free(this->mpv_context);
        this->mpv_context = nullptr;
    }
    if (this->mpv) {
        mpv_terminate_destroy(this->mpv);
        this->mpv = nullptr;
    }
#ifdef ANDROID
    deleteSurfaceObj();
#endif
}

void MPVCore::restart() {
    this->clean();
    this->init();
    setFrameSize(rect);
}
