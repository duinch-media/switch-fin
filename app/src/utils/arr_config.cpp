#include "utils/arr_config.hpp"
#include "utils/config.hpp"
#include <borealis/core/logger.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <mutex>
#include <vector>

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

std::string cleanUrl(std::string url) {
    while (!url.empty() && (url.back() == '/' || url.back() == ' ')) {
        url.pop_back();
    }
    return url;
}

std::string cleanKey(std::string key) {
    while (!key.empty() && (std::isspace(static_cast<unsigned char>(key.front())) || key.front() == '\r' || key.front() == '\n')) {
        key.erase(key.begin());
    }
    while (!key.empty() && (std::isspace(static_cast<unsigned char>(key.back())) || key.back() == '\r' || key.back() == '\n')) {
        key.pop_back();
    }
    return key;
}

std::string getPrimaryFilePath() {
    return AppConfig::instance().configDir() + "/arr_config.json";
}

std::vector<std::string> getCandidateFilePaths() {
    std::vector<std::string> paths;
    std::string configDir = AppConfig::instance().configDir();
    paths.push_back(configDir + "/arr_config.json");
    paths.push_back(configDir + "/arr.json");
    paths.push_back(configDir + "/config.json");
#ifdef __SWITCH__
    paths.push_back("sdmc:/switch/Switchfin/arr_config.json");
    paths.push_back("sdmc:/switch/Switchfin/arr.json");
    paths.push_back("sdmc:/switch/arr_config.json");
    paths.push_back("sdmc:/switch/arr.json");
#endif
    paths.push_back("./arr_config.json");
    return paths;
}

void parseJsonData(const nlohmann::json& j) {
    if (j.contains("radarr") && j["radarr"].is_object()) {
        const auto& r = j["radarr"];
        if (r.contains("url")) s_data.radarrUrl = cleanUrl(r["url"].get<std::string>());
        if (r.contains("api_key")) s_data.radarrKey = cleanKey(r["api_key"].get<std::string>());
        else if (r.contains("apiKey")) s_data.radarrKey = cleanKey(r["apiKey"].get<std::string>());
        else if (r.contains("key")) s_data.radarrKey = cleanKey(r["key"].get<std::string>());
        // arr_config.json writes rootFolder / qualityProfileId; root / profile are legacy
        if (r.contains("rootFolder")) s_data.radarrRoot = r["rootFolder"].get<std::string>();
        else if (r.contains("root")) s_data.radarrRoot = r["root"].get<std::string>();
        if (r.contains("qualityProfileId")) s_data.radarrProfile = r["qualityProfileId"].get<int>();
        else if (r.contains("profile")) s_data.radarrProfile = r["profile"].get<int>();
    }
    if (j.contains("sonarr") && j["sonarr"].is_object()) {
        const auto& s = j["sonarr"];
        if (s.contains("url")) s_data.sonarrUrl = cleanUrl(s["url"].get<std::string>());
        if (s.contains("api_key")) s_data.sonarrKey = cleanKey(s["api_key"].get<std::string>());
        else if (s.contains("apiKey")) s_data.sonarrKey = cleanKey(s["apiKey"].get<std::string>());
        else if (s.contains("key")) s_data.sonarrKey = cleanKey(s["key"].get<std::string>());
        // arr_config.json writes rootFolder / qualityProfileId; root / profile are legacy
        if (s.contains("rootFolder")) s_data.sonarrRoot = s["rootFolder"].get<std::string>();
        else if (s.contains("root")) s_data.sonarrRoot = s["root"].get<std::string>();
        if (s.contains("qualityProfileId")) s_data.sonarrProfile = s["qualityProfileId"].get<int>();
        else if (s.contains("profile")) s_data.sonarrProfile = s["profile"].get<int>();
    }

    if (j.contains("radarrUrl")) s_data.radarrUrl = cleanUrl(j["radarrUrl"].get<std::string>());
    else if (j.contains("radarr_url")) s_data.radarrUrl = cleanUrl(j["radarr_url"].get<std::string>());

    if (j.contains("radarrKey")) s_data.radarrKey = cleanKey(j["radarrKey"].get<std::string>());
    else if (j.contains("radarr_api_key")) s_data.radarrKey = cleanKey(j["radarr_api_key"].get<std::string>());
    else if (j.contains("radarr_key")) s_data.radarrKey = cleanKey(j["radarr_key"].get<std::string>());

    if (j.contains("radarrRoot")) s_data.radarrRoot = j["radarrRoot"].get<std::string>();
    else if (j.contains("radarr_root")) s_data.radarrRoot = j["radarr_root"].get<std::string>();

    if (j.contains("sonarrUrl")) s_data.sonarrUrl = cleanUrl(j["sonarrUrl"].get<std::string>());
    else if (j.contains("sonarr_url")) s_data.sonarrUrl = cleanUrl(j["sonarr_url"].get<std::string>());

    if (j.contains("sonarrKey")) s_data.sonarrKey = cleanKey(j["sonarrKey"].get<std::string>());
    else if (j.contains("sonarr_api_key")) s_data.sonarrKey = cleanKey(j["sonarr_api_key"].get<std::string>());
    else if (j.contains("sonarr_key")) s_data.sonarrKey = cleanKey(j["sonarr_key"].get<std::string>());

    if (j.contains("sonarrRoot")) s_data.sonarrRoot = j["sonarrRoot"].get<std::string>();
    else if (j.contains("sonarr_root")) s_data.sonarrRoot = j["sonarr_root"].get<std::string>();
}

void loadLocked() {
    if (s_loaded) return;
    s_loaded = true;

    auto paths = getCandidateFilePaths();
    for (const auto& path : paths) {
        std::ifstream file(path);
        if (file.is_open()) {
            try {
                nlohmann::json j;
                file >> j;
                parseJsonData(j);
                brls::Logger::info("Loaded Arr config from {}", path);
                break;
            } catch (const std::exception& e) {
                brls::Logger::warning("Failed to parse Arr config {}: {}", path, e.what());
            }
        }
    }
}

void saveLocked() {
    try {
        nlohmann::json j = s_data;
        std::string path = getPrimaryFilePath();
        std::ofstream file(path);
        if (file.is_open()) {
            file << j.dump(2);
            brls::Logger::info("Saved Arr config to {}", path);
        }
    } catch (...) {
    }
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
    s_data.radarrKey = cleanKey(key);
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
    s_data.sonarrKey = cleanKey(key);
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
