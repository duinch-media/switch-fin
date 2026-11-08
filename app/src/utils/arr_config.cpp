#include "utils/arr_config.hpp"
#include "utils/config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <mutex>

namespace {

struct ArrData {
    std::string radarrUrl;
    std::string radarrKey;
    std::string radarrRoot = "/movies";
    int radarrProfile = 1;

    std::string sonarrUrl;
    std::string sonarrKey;
    std::string sonarrRoot = "/tv";
    int sonarrProfile = 1;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    ArrData, radarrUrl, radarrKey, radarrRoot, radarrProfile,
    sonarrUrl, sonarrKey, sonarrRoot, sonarrProfile);

std::mutex s_mutex;
ArrData s_data;
bool s_loaded = false;

std::string getFilePath() {
    return AppConfig::instance().configDir() + "/arr_config.json";
}

void loadLocked() {
    if (s_loaded) return;
    s_loaded = true;
    std::ifstream file(getFilePath());
    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;
            s_data = j.get<ArrData>();
        } catch (...) {
            // Use defaults on error
        }
    }
}

void saveLocked() {
    try {
        nlohmann::json j = s_data;
        std::ofstream file(getFilePath());
        if (file.is_open()) {
            file << j.dump(2);
        }
    } catch (...) {
    }
}

std::string cleanUrl(std::string url) {
    while (!url.empty() && (url.back() == '/' || url.back() == ' ')) {
        url.pop_back();
    }
    return url;
}

} // namespace

std::string ArrConfig::getRadarrUrl() {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    return s_data.radarrUrl;
}

void ArrConfig::setRadarrUrl(const std::string& url) {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    s_data.radarrUrl = cleanUrl(url);
    saveLocked();
}

std::string ArrConfig::getRadarrKey() {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    return s_data.radarrKey;
}

void ArrConfig::setRadarrKey(const std::string& key) {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    s_data.radarrKey = key;
    saveLocked();
}

std::string ArrConfig::getRadarrRoot() {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    return s_data.radarrRoot.empty() ? "/movies" : s_data.radarrRoot;
}

void ArrConfig::setRadarrRoot(const std::string& root) {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    s_data.radarrRoot = root;
    saveLocked();
}

int ArrConfig::getRadarrProfile() {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    return s_data.radarrProfile <= 0 ? 1 : s_data.radarrProfile;
}

void ArrConfig::setRadarrProfile(int profileId) {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    s_data.radarrProfile = profileId;
    saveLocked();
}

std::string ArrConfig::getSonarrUrl() {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    return s_data.sonarrUrl;
}

void ArrConfig::setSonarrUrl(const std::string& url) {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    s_data.sonarrUrl = cleanUrl(url);
    saveLocked();
}

std::string ArrConfig::getSonarrKey() {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    return s_data.sonarrKey;
}

void ArrConfig::setSonarrKey(const std::string& key) {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    s_data.sonarrKey = key;
    saveLocked();
}

std::string ArrConfig::getSonarrRoot() {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    return s_data.sonarrRoot.empty() ? "/tv" : s_data.sonarrRoot;
}

void ArrConfig::setSonarrRoot(const std::string& root) {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    s_data.sonarrRoot = root;
    saveLocked();
}

int ArrConfig::getSonarrProfile() {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    return s_data.sonarrProfile <= 0 ? 1 : s_data.sonarrProfile;
}

void ArrConfig::setSonarrProfile(int profileId) {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    s_data.sonarrProfile = profileId;
    saveLocked();
}

bool ArrConfig::isRadarrConfigured() {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    return !s_data.radarrUrl.empty() && !s_data.radarrKey.empty();
}

bool ArrConfig::isSonarrConfigured() {
    std::lock_guard<std::mutex> lock(s_mutex);
    loadLocked();
    return !s_data.sonarrUrl.empty() && !s_data.sonarrKey.empty();
}
