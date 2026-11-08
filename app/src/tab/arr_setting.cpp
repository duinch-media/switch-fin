#include "tab/arr_setting.hpp"
#include "utils/arr_config.hpp"
#include "data/arr_client.hpp"
#include "utils/dialog.hpp"
#include <borealis/core/thread.hpp>

brls::View* ArrSetting::create() {
    return new ArrSetting();
}

ArrSetting::ArrSetting() {
    this->inflateFromXMLRes("xml/tabs/arr_setting.xml");

    // Radarr
    this->inputRadarrUrl->init(
        "Server URL", ArrConfig::getRadarrUrl(),
        [](std::string val) { ArrConfig::setRadarrUrl(val); },
        "http://ip:7878", "", 255);

    this->inputRadarrKey->init(
        "API Key", ArrConfig::getRadarrKey(),
        [](std::string val) { ArrConfig::setRadarrKey(val); },
        "Radarr API Key", "", 255);

    this->inputRadarrRoot->init(
        "Root Folder", ArrConfig::getRadarrRoot(),
        [](std::string val) { ArrConfig::setRadarrRoot(val); },
        "/movies", "", 255);

    this->btnRadarrTest->registerClickAction([this](...) {
        this->testRadarr();
        return true;
    });

    // Sonarr
    this->inputSonarrUrl->init(
        "Server URL", ArrConfig::getSonarrUrl(),
        [](std::string val) { ArrConfig::setSonarrUrl(val); },
        "http://ip:8989", "", 255);

    this->inputSonarrKey->init(
        "API Key", ArrConfig::getSonarrKey(),
        [](std::string val) { ArrConfig::setSonarrKey(val); },
        "Sonarr API Key", "", 255);

    this->inputSonarrRoot->init(
        "Root Folder", ArrConfig::getSonarrRoot(),
        [](std::string val) { ArrConfig::setSonarrRoot(val); },
        "/tv", "", 255);

    this->btnSonarrTest->registerClickAction([this](...) {
        this->testSonarr();
        return true;
    });
}

ArrSetting::~ArrSetting() = default;

void ArrSetting::testRadarr() {
    std::string url = ArrConfig::getRadarrUrl();
    std::string key = ArrConfig::getRadarrKey();
    if (url.empty() || key.empty()) {
        Dialog::show("Please fill in Radarr URL and API Key first.");
        return;
    }

    this->btnRadarrTest->setDetail("Testing...");
    brls::async([this, url, key]() {
        auto res = data::ArrClient::instance().testRadarr(url, key);
        brls::sync([this, res]() {
            if (res.isOk()) {
                this->btnRadarrTest->setDetail("Connected OK");
                Dialog::show(res.value());
            } else {
                this->btnRadarrTest->setDetail("Failed");
                Dialog::show("Radarr Connection Failed: " + res.error().message);
            }
        });
    });
}

void ArrSetting::testSonarr() {
    std::string url = ArrConfig::getSonarrUrl();
    std::string key = ArrConfig::getSonarrKey();
    if (url.empty() || key.empty()) {
        Dialog::show("Please fill in Sonarr URL and API Key first.");
        return;
    }

    this->btnSonarrTest->setDetail("Testing...");
    brls::async([this, url, key]() {
        auto res = data::ArrClient::instance().testSonarr(url, key);
        brls::sync([this, res]() {
            if (res.isOk()) {
                this->btnSonarrTest->setDetail("Connected OK");
                Dialog::show(res.value());
            } else {
                this->btnSonarrTest->setDetail("Failed");
                Dialog::show("Sonarr Connection Failed: " + res.error().message);
            }
        });
    });
}
