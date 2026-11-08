#include "data/arr_client.hpp"
#include "utils/arr_config.hpp"
#include "api/http.hpp"
#include <nlohmann/json.hpp>
#include <borealis/core/logger.hpp>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace data {

namespace {

std::string urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
        }
    }
    return escaped.str();
}

std::string extractPoster(const nlohmann::json& j) {
    if (j.contains("images") && j["images"].is_array()) {
        for (const auto& img : j["images"]) {
            if (img.value("coverType", "") == "poster") {
                return img.value("remoteUrl", img.value("url", ""));
            }
        }
    }
    return j.value("remotePoster", "");
}

domain::RequestStatus determineStatus(bool hasFile, int id, bool monitored) {
    if (hasFile) return domain::RequestStatus::AVAILABLE;
    if (id > 0 && monitored) return domain::RequestStatus::MONITORED;
    if (id > 0) return domain::RequestStatus::REQUESTED;
    return domain::RequestStatus::NOT_REQUESTED;
}

} // namespace

ArrClient& ArrClient::instance() {
    static ArrClient s_instance;
    return s_instance;
}

domain::Result<std::vector<domain::RequestMediaItem>> ArrClient::searchMovie(const std::string& query) {
    std::string baseUrl = ArrConfig::getRadarrUrl();
    std::string key = ArrConfig::getRadarrKey();
    if (baseUrl.empty() || key.empty()) {
        return domain::DomainError::unauthorized("Radarr is not configured");
    }

    std::string url = baseUrl + "/api/v3/movie/lookup?term=" + urlEncode(query);
    HTTP::Header headers = {"X-Api-Key: " + key, "Accept: application/json"};

    try {
        std::string resp = HTTP::get(url, headers, HTTP::Timeout{5000});
        nlohmann::json j = nlohmann::json::parse(resp);
        std::vector<domain::RequestMediaItem> results;

        if (j.is_array()) {
            for (const auto& item : j) {
                domain::RequestMediaItem m;
                m.type = domain::RequestType::MOVIE;
                m.title = item.value("title", "");
                m.originalTitle = item.value("originalTitle", m.title);
                m.overview = item.value("overview", "");
                m.year = item.value("year", 0);
                m.tmdbId = item.value("tmdbId", 0);
                m.imdbId = item.value("imdbId", "");
                m.titleSlug = item.value("titleSlug", "");
                m.id = item.value("id", 0);
                m.isMonitored = item.value("monitored", false);
                m.hasFile = item.value("hasFile", false);
                m.posterUrl = extractPoster(item);
                m.status = determineStatus(m.hasFile, m.id, m.isMonitored);
                results.push_back(std::move(m));
            }
        }
        return results;
    } catch (const std::exception& e) {
        brls::Logger::error("searchMovie error: {}", e.what());
        return domain::DomainError::network(e.what());
    }
}

domain::Result<std::vector<domain::RequestMediaItem>> ArrClient::searchSeries(const std::string& query) {
    std::string baseUrl = ArrConfig::getSonarrUrl();
    std::string key = ArrConfig::getSonarrKey();
    if (baseUrl.empty() || key.empty()) {
        return domain::DomainError::unauthorized("Sonarr is not configured");
    }

    std::string url = baseUrl + "/api/v3/series/lookup?term=" + urlEncode(query);
    HTTP::Header headers = {"X-Api-Key: " + key, "Accept: application/json"};

    try {
        std::string resp = HTTP::get(url, headers, HTTP::Timeout{5000});
        nlohmann::json j = nlohmann::json::parse(resp);
        std::vector<domain::RequestMediaItem> results;

        if (j.is_array()) {
            for (const auto& item : j) {
                domain::RequestMediaItem s;
                s.type = domain::RequestType::TV;
                s.title = item.value("title", "");
                s.overview = item.value("overview", "");
                s.year = item.value("year", 0);
                s.tvdbId = item.value("tvdbId", 0);
                s.imdbId = item.value("imdbId", "");
                s.titleSlug = item.value("titleSlug", "");
                s.id = item.value("id", 0);
                s.isMonitored = item.value("monitored", false);
                s.posterUrl = extractPoster(item);
                s.status = determineStatus(false, s.id, s.isMonitored);
                results.push_back(std::move(s));
            }
        }
        return results;
    } catch (const std::exception& e) {
        brls::Logger::error("searchSeries error: {}", e.what());
        return domain::DomainError::network(e.what());
    }
}

domain::Result<bool> ArrClient::requestMovie(
    const domain::RequestMediaItem& item, int qualityProfileId, const std::string& rootFolder) {
    std::string baseUrl = ArrConfig::getRadarrUrl();
    std::string key = ArrConfig::getRadarrKey();
    if (baseUrl.empty() || key.empty()) {
        return domain::DomainError::unauthorized("Radarr is not configured");
    }

    std::string url = baseUrl + "/api/v3/movie";
    HTTP::Header headers = {"X-Api-Key: " + key, "Content-Type: application/json"};

    nlohmann::json body = {
        {"title", item.title},
        {"qualityProfileId", qualityProfileId > 0 ? qualityProfileId : ArrConfig::getRadarrProfile()},
        {"titleSlug", item.titleSlug.empty() ? item.title : item.titleSlug},
        {"tmdbId", item.tmdbId},
        {"year", item.year},
        {"rootFolderPath", rootFolder.empty() ? ArrConfig::getRadarrRoot() : rootFolder},
        {"monitored", true},
        {"addOptions", {{"searchForMovie", true}}},
    };

    try {
        std::string resp = HTTP::post(url, body.dump(), headers, HTTP::Timeout{8000});
        nlohmann::json j = nlohmann::json::parse(resp);
        if (j.contains("id") && j["id"].get<int>() > 0) {
            return true;
        }
        return domain::DomainError::network("Radarr returned unexpected response");
    } catch (const std::exception& e) {
        brls::Logger::error("requestMovie error: {}", e.what());
        return domain::DomainError::network(e.what());
    }
}

domain::Result<bool> ArrClient::requestSeries(
    const domain::RequestMediaItem& item, int qualityProfileId, const std::string& rootFolder) {
    std::string baseUrl = ArrConfig::getSonarrUrl();
    std::string key = ArrConfig::getSonarrKey();
    if (baseUrl.empty() || key.empty()) {
        return domain::DomainError::unauthorized("Sonarr is not configured");
    }

    std::string url = baseUrl + "/api/v3/series";
    HTTP::Header headers = {"X-Api-Key: " + key, "Content-Type: application/json"};

    nlohmann::json body = {
        {"title", item.title},
        {"qualityProfileId", qualityProfileId > 0 ? qualityProfileId : ArrConfig::getSonarrProfile()},
        {"titleSlug", item.titleSlug.empty() ? item.title : item.titleSlug},
        {"tvdbId", item.tvdbId},
        {"year", item.year},
        {"rootFolderPath", rootFolder.empty() ? ArrConfig::getSonarrRoot() : rootFolder},
        {"monitored", true},
        {"seasonFolder", true},
        {"addOptions", {{"searchForMissingEpisodes", true}}},
    };

    try {
        std::string resp = HTTP::post(url, body.dump(), headers, HTTP::Timeout{8000});
        nlohmann::json j = nlohmann::json::parse(resp);
        if (j.contains("id") && j["id"].get<int>() > 0) {
            return true;
        }
        return domain::DomainError::network("Sonarr returned unexpected response");
    } catch (const std::exception& e) {
        brls::Logger::error("requestSeries error: {}", e.what());
        return domain::DomainError::network(e.what());
    }
}

domain::Result<domain::RequestMediaItem> ArrClient::checkMovieStatus(int tmdbId) {
    std::string baseUrl = ArrConfig::getRadarrUrl();
    std::string key = ArrConfig::getRadarrKey();
    if (baseUrl.empty() || key.empty()) {
        return domain::DomainError::unauthorized("Radarr is not configured");
    }

    std::string url = baseUrl + "/api/v3/movie/lookup/tmdb?tmdbId=" + std::to_string(tmdbId);
    HTTP::Header headers = {"X-Api-Key: " + key, "Accept: application/json"};

    try {
        std::string resp = HTTP::get(url, headers, HTTP::Timeout{5000});
        nlohmann::json item = nlohmann::json::parse(resp);
        domain::RequestMediaItem m;
        m.type = domain::RequestType::MOVIE;
        m.title = item.value("title", "");
        m.overview = item.value("overview", "");
        m.year = item.value("year", 0);
        m.tmdbId = item.value("tmdbId", tmdbId);
        m.id = item.value("id", 0);
        m.isMonitored = item.value("monitored", false);
        m.hasFile = item.value("hasFile", false);
        m.status = determineStatus(m.hasFile, m.id, m.isMonitored);
        return m;
    } catch (const std::exception& e) {
        return domain::DomainError::network(e.what());
    }
}

domain::Result<domain::RequestMediaItem> ArrClient::checkSeriesStatus(int tvdbId) {
    std::string baseUrl = ArrConfig::getSonarrUrl();
    std::string key = ArrConfig::getSonarrKey();
    if (baseUrl.empty() || key.empty()) {
        return domain::DomainError::unauthorized("Sonarr is not configured");
    }

    std::string url = baseUrl + "/api/v3/series/lookup?term=tvdb:" + std::to_string(tvdbId);
    HTTP::Header headers = {"X-Api-Key: " + key, "Accept: application/json"};

    try {
        std::string resp = HTTP::get(url, headers, HTTP::Timeout{5000});
        nlohmann::json j = nlohmann::json::parse(resp);
        if (j.is_array() && !j.empty()) {
            const auto& item = j.front();
            domain::RequestMediaItem s;
            s.type = domain::RequestType::TV;
            s.title = item.value("title", "");
            s.year = item.value("year", 0);
            s.tvdbId = item.value("tvdbId", tvdbId);
            s.id = item.value("id", 0);
            s.isMonitored = item.value("monitored", false);
            s.status = determineStatus(false, s.id, s.isMonitored);
            return s;
        }
        return domain::DomainError::notFound();
    } catch (const std::exception& e) {
        return domain::DomainError::network(e.what());
    }
}

domain::Result<std::string> ArrClient::testRadarr(const std::string& url, const std::string& key) {
    std::string testUrl = url + "/api/v3/system/status";
    HTTP::Header headers = {"X-Api-Key: " + key, "Accept: application/json"};
    try {
        std::string resp = HTTP::get(testUrl, headers, HTTP::Timeout{4000});
        nlohmann::json j = nlohmann::json::parse(resp);
        std::string ver = j.value("version", "OK");
        return "Connected to Radarr v" + ver;
    } catch (const std::exception& e) {
        return domain::DomainError::network(e.what());
    }
}

domain::Result<std::string> ArrClient::testSonarr(const std::string& url, const std::string& key) {
    std::string testUrl = url + "/api/v3/system/status";
    HTTP::Header headers = {"X-Api-Key: " + key, "Accept: application/json"};
    try {
        std::string resp = HTTP::get(testUrl, headers, HTTP::Timeout{4000});
        nlohmann::json j = nlohmann::json::parse(resp);
        std::string ver = j.value("version", "OK");
        return "Connected to Sonarr v" + ver;
    } catch (const std::exception& e) {
        return domain::DomainError::network(e.what());
    }
}

} // namespace data
