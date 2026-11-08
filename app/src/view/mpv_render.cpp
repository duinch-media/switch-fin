#include "view/mpv_core.hpp"
#include <algorithm>
#include "utils/misc.hpp"

#ifndef MPV_SW_RENDER
#ifdef BOREALIS_USE_D3D11
#include <borealis/platforms/driver/d3d11.hpp>
extern std::unique_ptr<brls::D3D11Context> D3D11_CONTEXT;
#elif defined(BOREALIS_USE_DEKO3D)
#include <borealis/platforms/switch/switch_video.hpp>
#elif defined(BOREALIS_USE_GXM)
#include <borealis/platforms/psv/psv_video.hpp>
#include <borealis/extern/nanovg/nanovg_gxm.h>
#endif
#endif

void configureMpvPlatformBuffering(mpv_handle *mpv) {
    int effectiveCacheMb = MPVCore::INMEMORY_CACHE;
#if defined(__PSV__) || defined(TRIMUI)
    const int maxBackMb = 8;
    const char *readaheadSecs = "30";
    const char *pauseWaitSecs = "3";
    const char *streamBufSize = "512KiB";
#elif defined(__SWITCH__)
    const bool isAppMode = brls::Application::getPlatform()->isApplicationMode();
    if (effectiveCacheMb == 10) effectiveCacheMb = isAppMode ? 100 : 50;
    if (!isAppMode && effectiveCacheMb > 50) {
        brls::Logger::warning("Switch Applet mode detected: clamping cache from {}MB to 50MB", effectiveCacheMb);
        effectiveCacheMb = 50;
    }
    const int maxBackMb = isAppMode ? 25 : 10;
    const char *readaheadSecs = isAppMode ? "60" : "30";
    const char *pauseWaitSecs = "4";
    const char *streamBufSize = "2MiB";
#elif defined(__PS4__) || defined(ANDROID)
    if (effectiveCacheMb == 10) effectiveCacheMb = 100;
    const int maxBackMb = 32;
    const char *readaheadSecs = "90";
    const char *pauseWaitSecs = "3";
    const char *streamBufSize = "2MiB";
#else
    if (effectiveCacheMb == 10) effectiveCacheMb = 200;
    const int maxBackMb = 50;
    const char *readaheadSecs = "120";
    const char *pauseWaitSecs = "3";
    const char *streamBufSize = "4MiB";
#endif

    mpv_set_option_string(mpv, "stream-buffer-size", streamBufSize);
    mpv_set_option_string(mpv, "stream-lavf-o", "reconnect=1,reconnect_streamed=1,reconnect_delay_max=5");
    mpv_set_option_string(mpv, "cache-pause", "yes");
    mpv_set_option_string(mpv, "cache-pause-wait", pauseWaitSecs);
    mpv_set_option_string(mpv, "demuxer-donate-buffer", "yes");

    if (effectiveCacheMb > 0) {
        const int backBytesMb = std::clamp(effectiveCacheMb / 5, 2, maxBackMb);
        const int forwardBytesMb = std::max(effectiveCacheMb - backBytesMb, 8);
        brls::Logger::info("MPV smart cache: total={}MiB (forward={}MiB, back={}MiB, readahead={}s, wait={}s)",
            effectiveCacheMb, forwardBytesMb, backBytesMb, readaheadSecs, pauseWaitSecs);
        mpv_set_option_string(mpv, "cache", "yes");
        mpv_set_option_string(mpv, "demuxer-max-bytes", fmt::format("{}MiB", forwardBytesMb).c_str());
        mpv_set_option_string(mpv, "demuxer-max-back-bytes", fmt::format("{}MiB", backBytesMb).c_str());
        mpv_set_option_string(mpv, "demuxer-readahead-secs", readaheadSecs);
        mpv_set_option_string(mpv, "cache-secs", readaheadSecs);
    } else {
        mpv_set_option_string(mpv, "cache", "auto");
        mpv_set_option_string(mpv, "demuxer-max-bytes", "16MiB");
        mpv_set_option_string(mpv, "demuxer-max-back-bytes", "4MiB");
    }
}

void MPVCore::on_update(void *self) {
    MPVCore *mpv = reinterpret_cast<MPVCore *>(self);
    brls::sync([mpv]() {
        uint64_t flags = mpv_render_context_update(mpv->mpv_context);
#if defined(MPV_SW_RENDER) || defined(BOREALIS_USE_GXM)
        if (flags & MPV_RENDER_UPDATE_FRAME) {
            mpv_render_context_render(mpv->mpv_context, mpv->mpv_params);
            mpv_render_context_report_swap(mpv->mpv_context);
        }
#else
        (void)flags;
#endif
    });
}

MPVMap MPVCore::supportCodecs() {
    MPVMap codecs;
    mpv_node node;
    mpv_get_property(mpv, "decoder-list", MPV_FORMAT_NODE, &node);
    if (node.format == MPV_FORMAT_NODE_ARRAY) {
        mpv_node_list *codec_list = node.u.list;
        for (int i = 0; i < codec_list->num; i++) {
            if (codec_list->values[i].format == MPV_FORMAT_NODE_MAP) {
                mpv_node_list *codec_map = codec_list->values[i].u.list;
                std::string name, desc;
                for (int n = 0; n < codec_map->num; n++) {
                    char *key = codec_map->keys[n];
                    if (strcmp(key, "codec") == 0) {
                        name = codec_map->values[n].u.string;
                    } else if (strcmp(key, "description") == 0) {
                        desc = codec_map->values[n].u.string;
                    }
                }
                codecs.insert(std::make_pair(name, desc));
            }
        }
    }
    mpv_free_node_contents(&node);
    return codecs;
}

void MPVCore::setFrameSize(brls::Rect area) {
    rect = area;
    if (std::isnan(rect.getWidth()) || std::isnan(rect.getHeight())) return;

#ifdef MPV_SW_RENDER
#ifdef BOREALIS_USE_D3D11
    const static int mpvImageFlags = NVG_IMAGE_STREAMING | NVG_IMAGE_COPY_SWAP;
#else
    const static int mpvImageFlags = 0;
#endif
    int drawWidth = rect.getWidth() * brls::Application::windowScale;
    int drawHeight = rect.getHeight() * brls::Application::windowScale;
    if (drawWidth == 0 || drawHeight == 0) return;
    int frameSize = drawWidth * drawHeight;

    if (pixels != nullptr && frameSize > sw_size[0] * sw_size[1]) {
        brls::Logger::debug("Enlarge video surface buffer");
        free(pixels);
        pixels = nullptr;
    }

    if (pixels == nullptr) {
        pixels = malloc(frameSize * PIXCEL_SIZE);
        mpv_params[3].data = pixels;
    }

    auto vg = brls::Application::getNVGContext();
    if (nvg_image) nvgDeleteImage(vg, nvg_image);
    nvg_image = nvgCreateImageRGBA(vg, drawWidth, drawHeight, mpvImageFlags, (const unsigned char *)pixels);

    sw_size[0] = drawWidth;
    sw_size[1] = drawHeight;
    pitch = PIXCEL_SIZE * drawWidth;
#elif defined(BOREALIS_USE_GXM)
    brls::sync([this]() {
        mpv_render_context_render(this->mpv_context, mpv_params);
        mpv_render_context_report_swap(this->mpv_context);
    });
#elif !defined(BOREALIS_USE_D3D11)
    this->mpv_fbo.w = brls::Application::windowWidth;
    this->mpv_fbo.h = brls::Application::windowHeight;
#endif
}

bool MPVCore::isValid() {
#ifdef ANDROID
    return true;
#else
    return mpv_context != nullptr;
#endif
}

void MPVCore::draw(brls::Rect area, float alpha) {
    if (mpv_context == nullptr) return;
    if (!(this->rect == area)) this->setFrameSize(area);

#ifdef MPV_SW_RENDER
    if (!pixels) return;

    auto *vg = brls::Application::getNVGContext();
    nvgUpdateImage(vg, nvg_image, (const unsigned char *)pixels);

    nvgBeginPath(vg);
    nvgFillColor(vg, NVGcolor{0, 0, 0, alpha});
    nvgRect(vg, rect.getMinX(), rect.getMinY(), rect.getWidth(), rect.getHeight());
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRect(vg, rect.getMinX(), rect.getMinY(), rect.getWidth(), rect.getHeight());
    nvgFillPaint(vg, nvgImagePattern(vg, 0, 0, rect.getWidth(), rect.getHeight(), 0, nvg_image, alpha));
    nvgFill(vg);
#elif defined(BOREALIS_USE_GXM)
    NVGcontext *vg = brls::Application::getNVGContext();
    NVGpaint img =
        nvgImagePattern(vg, area.getMinX(), area.getMinY(), area.getWidth(), area.getHeight(), 0, nvg_image, alpha);
    nvgBeginPath(vg);
    nvgRect(vg, area.getMinX(), area.getMinY(), area.getWidth(), area.getHeight());
    nvgFillPaint(vg, img);
    nvgFill(vg);
#elif defined(ANDROID)
#else
    if (alpha >= 1 && !this->video_stopped) {
#ifdef BOREALIS_USE_D3D11
        ID3D11Texture2D *tex = nullptr;
        auto swapChain = D3D11_CONTEXT->getSwapChain();
        swapChain->GetBuffer(0, IID_PPV_ARGS(&tex));
        mpv_fbo.tex = tex;
#elif defined(BOREALIS_USE_DEKO3D)
        static auto videoContext =
            dynamic_cast<brls::SwitchVideoContext *>(brls::Application::getPlatform()->getVideoContext());
        this->mpv_fbo.tex = videoContext->getFramebuffer();
        videoContext->queueSignalFence(&readyFence);
        videoContext->queueFlush();
#endif
        mpv_render_context_render(this->mpv_context, mpv_params);
#ifdef BOREALIS_USE_D3D11
        tex->Release();
        D3D11_CONTEXT->beginFrame();
#elif defined(BOREALIS_USE_DEKO3D)
        videoContext->queueWaitFence(&doneFence);
#elif defined(BOREALIS_USE_OPENGL)
        glBindFramebuffer(GL_FRAMEBUFFER, default_framebuffer);
        glViewport(0, 0, brls::Application::windowWidth, brls::Application::windowHeight);
#endif
        mpv_render_context_report_swap(this->mpv_context);
    }
#endif
}
