#include "tab/arr_setting.hpp"
#include "utils/arr_config.hpp"
#include "data/arr_client.hpp"
#include "data/arr_scanner.hpp"
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

    this->btnRadarrScan->registerClickAction([this](...) {
        this->scanRadarr();
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

    this->btnSonarrScan->registerClickAction([this](...) {
        this->scanSonarr();
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

    this->btnRadarrTest->setDetailText("Testing...");
    brls::async([this, url, key]() {
        auto res = data::ArrClient::instance().testRadarr(url, key);
        brls::sync([this, res]() {
            if (res.isOk()) {
                this->btnRadarrTest->setDetailText("Connected OK");
                Dialog::show(res.value());
            } else {
                this->btnRadarrTest->setDetailText("Failed");
                Dialog::show("Radarr Connection Failed: " + res.error().message);
            }
        });
    });
}

void ArrSetting::scanRadarr() {
    this->btnRadarrScan->setDetailText("Scanning LAN...");
    std::string currentKey = ArrConfig::getRadarrKey();

    data::ArrScanner::scan(
        data::DiscoveredArr::RADARR, currentKey,
        [this](const data::DiscoveredArr& d) {
            this->inputRadarrUrl->setValue(d.url);
            ArrConfig::setRadarrUrl(d.url);
            std::string status = "Found: " + d.url + (d.authOk ? " (Auth OK)" : " (Need Key)");
            this->btnRadarrScan->setDetailText(status);
            this->btnRadarrTest->setDetailText(d.authOk ? "Connected OK" : "Verify Key");
            Dialog::show("Discovered Radarr at " + d.url + (d.authOk ? "\nAPI Key authenticated successfully!" : "\nPlease enter API Key to finish setup."));
        },
        [this](bool found) {
            if (!found) {
                this->btnRadarrScan->setDetailText("Not found");
                Dialog::show("No Radarr server found on LAN (:7878).\nCheck if Radarr is running on your network.");
            }
        });
}

void ArrSetting::testSonarr() {
    std::string url = ArrConfig::getSonarrUrl();
    std::string key = ArrConfig::getSonarrKey();
    if (url.empty() || key.empty()) {
        Dialog::show("Please fill in Sonarr URL and API Key first.");
        return;
    }

    this->btnSonarrTest->setDetailText("Testing...");
    brls::async([this, url, key]() {
        auto res = data::ArrClient::instance().testSonarr(url, key);
        brls::sync([this, res]() {
            if (res.isOk()) {
                this->btnSonarrTest->setDetailText("Connected OK");
                Dialog::show(res.value());
            } else {
                this->btnSonarrTest->setDetailText("Failed");
                Dialog::show("Sonarr Connection Failed: " + res.error().message);
            }
        });
    });
}

void ArrSetting::scanSonarr() {
    this->btnSonarrScan->setDetailText("Scanning LAN...");
    std::string currentKey = ArrConfig::getSonarrKey();

    data::ArrScanner::scan(
        data::DiscoveredArr::SONARR, currentKey,
        [this](const data::DiscoveredArr& d) {
            this->inputSonarrUrl->setValue(d.url);
            ArrConfig::setSonarrUrl(d.url);
            std::string status = "Found: " + d.url + (d.authOk ? " (Auth OK)" : " (Need Key)");
            this->btnSonarrScan->setDetailText(status);
            this->btnSonarrTest->setDetailText(d.authOk ? "Connected OK" : "Verify Key");
            Dialog::show("Discovered Sonarr at " + d.url + (d.authOk ? "\nAPI Key authenticated successfully!" : "\nPlease enter API Key to finish setup."));
        },
        [this](bool found) {
            if (!found) {
                this->btnSonarrScan->setDetailText("Not found");
                Dialog::show("No Sonarr server found on LAN (:8989).\nCheck if Sonarr is running on your network.");
            }
        });
}
