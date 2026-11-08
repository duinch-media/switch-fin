#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace domain {

enum class MediaType {
    UNKNOWN = 0,
    FOLDER,
    MOVIE,
    SERIES,
    SEASON,
    EPISODE,
    AUDIO,
    MUSIC_ALBUM,
    PLAYLIST,
    BOOK,
    LIVE_TV,
};

struct MediaStreamInfo {
    std::string index;
    std::string codec;
    std::string title;
    std::string language;
    bool isDefault = false;
};

struct MediaItemEntity {
    std::string id;
    std::string name;
    std::string overview;
    MediaType type = MediaType::UNKNOWN;
    uint64_t runTimeTicks = 0;
    int64_t playbackPositionTicks = 0;
    int productionYear = 0;
    float communityRating = 0.0f;
    bool isFavorite = false;
    bool isPlayed = false;

    // Image IDs
    std::string primaryImageTag;
    std::string thumbImageTag;
    std::string backdropImageTag;

    // Media sources and streams
    std::vector<MediaStreamInfo> audioStreams;
    std::vector<MediaStreamInfo> subtitleStreams;
};

} // namespace domain
