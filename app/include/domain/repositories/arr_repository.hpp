#pragma once

#include "domain/error.hpp"
#include "domain/models/request_item.hpp"
#include <string>
#include <vector>

namespace domain {

class IArrRepository {
public:
    virtual ~IArrRepository() = default;

    virtual Result<std::vector<RequestMediaItem>> searchMovie(const std::string& query) = 0;
    virtual Result<std::vector<RequestMediaItem>> searchSeries(const std::string& query) = 0;
    virtual Result<bool> requestMovie(const RequestMediaItem& item, int qualityProfileId, const std::string& rootFolder) = 0;
    virtual Result<bool> requestSeries(const RequestMediaItem& item, int qualityProfileId, const std::string& rootFolder) = 0;
    virtual Result<RequestMediaItem> checkMovieStatus(int tmdbId) = 0;
    virtual Result<RequestMediaItem> checkSeriesStatus(int tvdbId) = 0;
};

} // namespace domain
