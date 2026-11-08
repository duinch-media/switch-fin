#pragma once

#include <string>
#include <vector>

namespace domain {

enum class RequestType {
    MOVIE = 0,
    TV = 1,
};

enum class RequestStatus {
    NOT_REQUESTED = 0,
    REQUESTED,
    MONITORED,
    AVAILABLE,
    DOWNLOADING,
};

struct RequestMediaItem {
    std::string title;
    std::string originalTitle;
    std::string overview;
    std::string posterUrl;
    std::string titleSlug;
    RequestType type = RequestType::MOVIE;
    RequestStatus status = RequestStatus::NOT_REQUESTED;

    int year = 0;
    int tmdbId = 0;
    int tvdbId = 0;
    std::string imdbId;
    int id = 0; // > 0 if already in Radarr/Sonarr

    bool isMonitored = false;
    bool hasFile = false;
};

} // namespace domain
