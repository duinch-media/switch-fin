#pragma once

#include "domain/error.hpp"
#include "domain/models/media_item.hpp"
#include <string>
#include <vector>
#include <functional>

namespace domain {

class IMediaRepository {
public:
    virtual ~IMediaRepository() = default;

    virtual Result<std::vector<MediaItemEntity>> getLatestItems(const std::string& parentId) = 0;
    virtual Result<std::vector<MediaItemEntity>> getResumeItems() = 0;
    virtual Result<MediaItemEntity> getItemDetails(const std::string& itemId) = 0;
    virtual Result<std::vector<MediaItemEntity>> searchItems(const std::string& query) = 0;
    virtual Result<bool> reportPlaybackProgress(const std::string& itemId, int64_t positionTicks, bool isPaused) = 0;
};

} // namespace domain
