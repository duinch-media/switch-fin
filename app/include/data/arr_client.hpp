#pragma once

#include "domain/repositories/arr_repository.hpp"
#include <string>
#include <vector>

namespace data {

class ArrClient : public domain::IArrRepository {
public:
    static ArrClient& instance();

    domain::Result<std::vector<domain::RequestMediaItem>> searchMovie(const std::string& query) override;
    domain::Result<std::vector<domain::RequestMediaItem>> searchSeries(const std::string& query) override;
    domain::Result<bool> requestMovie(const domain::RequestMediaItem& item, int qualityProfileId, const std::string& rootFolder) override;
    domain::Result<bool> requestSeries(const domain::RequestMediaItem& item, int qualityProfileId, const std::string& rootFolder) override;
    domain::Result<domain::RequestMediaItem> checkMovieStatus(int tmdbId) override;
    domain::Result<domain::RequestMediaItem> checkSeriesStatus(int tvdbId) override;

    domain::Result<std::string> testRadarr(const std::string& url, const std::string& key);
    domain::Result<std::string> testSonarr(const std::string& url, const std::string& key);
};

} // namespace data
