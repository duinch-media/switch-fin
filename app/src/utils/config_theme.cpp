#include <borealis.hpp>
#include <sstream>
#include "utils/config.hpp"

void AppConfig::addColor(const brls::ThemeVariant tv, const std::string& name, NVGcolor defaultColor) {
    auto& theme = (tv == brls::ThemeVariant::LIGHT) ? brls::Theme::getLightTheme() : brls::Theme::getDarkTheme();

    if (!setting.contains(name)) {
        theme.addColor(name, defaultColor);
    } else {
        unsigned int r = 0, g = 0, b = 0;
        std::string s = setting.at(name).get<std::string>();

        std::stringstream sr{s.substr(1, 2)};
        sr >> std::hex >> r;
        std::stringstream sg{s.substr(3, 2)};
        sg >> std::hex >> g;
        std::stringstream sb{s.substr(5, 2)};
        sb >> std::hex >> b;
        theme.addColor(name, nvgRGB(r, g, b));
    }
}

void AppConfig::initThemes() {
    this->addColor(brls::ThemeVariant::LIGHT, "color/app", nvgRGB(2, 176, 183));
    this->addColor(brls::ThemeVariant::DARK, "color/app", nvgRGB(51, 186, 227));
    // metadata pills (detail pages)
    this->addColor(brls::ThemeVariant::LIGHT, "color/pill", nvgRGBA(0, 0, 0, 18));
    this->addColor(brls::ThemeVariant::DARK, "color/pill", nvgRGBA(255, 255, 255, 22));
    // surfaces placed over the background (content cards, PIN code panel...)
    this->addColor(brls::ThemeVariant::LIGHT, "color/surface", nvgRGB(255, 255, 255));
    this->addColor(brls::ThemeVariant::DARK, "color/surface", nvgRGB(22, 24, 29));
    // skeleton background colors
    this->addColor(brls::ThemeVariant::LIGHT, "color/grey_1", nvgRGB(245, 246, 247));
    this->addColor(brls::ThemeVariant::DARK, "color/grey_1", nvgRGB(51, 52, 53));
    this->addColor(brls::ThemeVariant::LIGHT, "color/grey_2", nvgRGB(245, 245, 245));
    this->addColor(brls::ThemeVariant::DARK, "color/grey_2", nvgRGB(51, 53, 55));
    this->addColor(brls::ThemeVariant::LIGHT, "color/grey_3", nvgRGBA(200, 200, 200, 16));
    this->addColor(brls::ThemeVariant::DARK, "color/grey_3", nvgRGBA(160, 160, 160, 160));
    this->addColor(brls::ThemeVariant::LIGHT, "color/danger", nvgRGB(198, 28, 28));
    this->addColor(brls::ThemeVariant::DARK, "color/danger", nvgRGBA(198, 28, 28, 180));
    this->addColor(brls::ThemeVariant::LIGHT, "color/white", nvgRGB(255, 255, 255));
    this->addColor(brls::ThemeVariant::DARK, "color/white", nvgRGBA(255, 255, 255, 180));
    this->addColor(brls::ThemeVariant::LIGHT, "color/line", nvgRGB(208, 208, 208));
    this->addColor(brls::ThemeVariant::DARK, "color/line", nvgRGB(100, 100, 100));
    this->addColor(brls::ThemeVariant::LIGHT, "font/grey", nvgRGB(148, 153, 160));
    this->addColor(brls::ThemeVariant::DARK, "font/grey", nvgRGB(148, 153, 160));

    if (brls::Application::ORIGINAL_WINDOW_HEIGHT == 544) {
        brls::getStyle().addMetric("app/album/height", 215);
        brls::getStyle().addMetric("app/books/height", 270);
        brls::getStyle().addMetric("app/video/height", 290);
        brls::getStyle().addMetric("app/card/poster/width", 150);
        brls::getStyle().addMetric("app/card/poster/row", 280);
        brls::getStyle().addMetric("app/card/wide/width", 280);
        brls::getStyle().addMetric("app/card/wide/row", 213);
        brls::getStyle().addMetric("app/grid/6", 5);
        brls::getStyle().addMetric("app/grid/5", 4);
        brls::getStyle().addMetric("app/grid/4", 3);
        brls::getStyle().addMetric("app/grid/3", 2);
        brls::getStyle().addMetric("app/grid/2", 1);
        brls::getStyle().addMetric("brls/sidebar/item_height", 46);
        brls::getStyle().addMetric("brls/sidebar/item_font_size", 15);
        brls::getStyle().addMetric("brls/tab_frame/content_padding_sides", 30);
        brls::getStyle().addMetric("main/content_padding_sides", 15);
        brls::getStyle().addMetric("main/content_padding_top_bottom", 20);
    } else {
        switch (brls::Application::ORIGINAL_WINDOW_HEIGHT) {
        case 1080:
            brls::getStyle().addMetric("app/album/height", 250);
            brls::getStyle().addMetric("app/books/height", 320);
            brls::getStyle().addMetric("app/video/height", 340);
            brls::getStyle().addMetric("app/card/poster/width", 225);
            brls::getStyle().addMetric("app/card/poster/row", 393);
            brls::getStyle().addMetric("app/card/wide/width", 410);
            brls::getStyle().addMetric("app/card/wide/row", 286);
            brls::getStyle().addMetric("app/grid/6", 8);
            brls::getStyle().addMetric("app/grid/5", 7);
            brls::getStyle().addMetric("app/grid/4", 6);
            brls::getStyle().addMetric("app/grid/3", 5);
            brls::getStyle().addMetric("app/grid/2", 4);
            brls::getStyle().addMetric("brls/sidebar/item_height", 64);
            brls::getStyle().addMetric("brls/sidebar/item_font_size", 20);
            break;
        case 900:
            brls::getStyle().addMetric("app/album/height", 240);
            brls::getStyle().addMetric("app/books/height", 305);
            brls::getStyle().addMetric("app/video/height", 325);
            brls::getStyle().addMetric("app/card/poster/width", 205);
            brls::getStyle().addMetric("app/card/poster/row", 363);
            brls::getStyle().addMetric("app/card/wide/width", 375);
            brls::getStyle().addMetric("app/card/wide/row", 266);
            brls::getStyle().addMetric("app/grid/6", 7);
            brls::getStyle().addMetric("app/grid/5", 6);
            brls::getStyle().addMetric("app/grid/4", 5);
            brls::getStyle().addMetric("app/grid/3", 4);
            brls::getStyle().addMetric("app/grid/2", 3);
            brls::getStyle().addMetric("brls/sidebar/item_height", 58);
            brls::getStyle().addMetric("brls/sidebar/item_font_size", 18);
            break;
        default:
            brls::getStyle().addMetric("app/album/height", 225);
            brls::getStyle().addMetric("app/books/height", 280);
            brls::getStyle().addMetric("app/video/height", 300);
            brls::getStyle().addMetric("app/card/poster/width", 185);
            brls::getStyle().addMetric("app/card/poster/row", 333);
            brls::getStyle().addMetric("app/card/wide/width", 340);
            brls::getStyle().addMetric("app/card/wide/row", 246);
            brls::getStyle().addMetric("app/grid/6", 6);
            brls::getStyle().addMetric("app/grid/5", 5);
            brls::getStyle().addMetric("app/grid/4", 4);
            brls::getStyle().addMetric("app/grid/3", 3);
            brls::getStyle().addMetric("app/grid/2", 2);
            brls::getStyle().addMetric("brls/sidebar/item_height", 54);
            brls::getStyle().addMetric("brls/sidebar/item_font_size", 17);
        }
        brls::getStyle().addMetric("main/content_padding_sides", 25);
        brls::getStyle().addMetric("main/content_padding_top_bottom", 30);
    }
}
