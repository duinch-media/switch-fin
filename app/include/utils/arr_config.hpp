#pragma once

#include <string>

class ArrConfig {
public:
    static std::string getRadarrUrl();
    static void setRadarrUrl(const std::string& url);

    static std::string getRadarrKey();
    static void setRadarrKey(const std::string& key);

    static std::string getRadarrRoot();
    static void setRadarrRoot(const std::string& root);

    static int getRadarrProfile();
    static void setRadarrProfile(int profileId);

    static std::string getSonarrUrl();
    static void setSonarrUrl(const std::string& url);

    static std::string getSonarrKey();
    static void setSonarrKey(const std::string& key);

    static std::string getSonarrRoot();
    static void setSonarrRoot(const std::string& root);

    static int getSonarrProfile();
    static void setSonarrProfile(int profileId);

    static bool isRadarrConfigured();
    static bool isSonarrConfigured();
};
