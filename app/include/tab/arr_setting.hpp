#pragma once

#include <borealis.hpp>

class ArrSetting : public brls::Box {
public:
    ArrSetting();
    ~ArrSetting() override;

    static brls::View* create();

private:
    BRLS_BIND(brls::InputCell, inputRadarrUrl, "setting/radarr/url");
    BRLS_BIND(brls::InputCell, inputRadarrKey, "setting/radarr/key");
    BRLS_BIND(brls::InputCell, inputRadarrRoot, "setting/radarr/root");
    BRLS_BIND(brls::DetailCell, btnRadarrTest, "setting/radarr/test");

    BRLS_BIND(brls::InputCell, inputSonarrUrl, "setting/sonarr/url");
    BRLS_BIND(brls::InputCell, inputSonarrKey, "setting/sonarr/key");
    BRLS_BIND(brls::InputCell, inputSonarrRoot, "setting/sonarr/root");
    BRLS_BIND(brls::DetailCell, btnSonarrTest, "setting/sonarr/test");

    void testRadarr();
    void testSonarr();
};
