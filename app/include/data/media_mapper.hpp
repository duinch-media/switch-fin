#pragma once

#include "domain/models/media_item.hpp"
#include "api/jellyfin/media.hpp"

namespace data {

class MediaMapper {
public:
    static domain::MediaType mapType(const std::string& typeStr) {
        if (typeStr == jellyfin::mediaTypeFolder) return domain::MediaType::FOLDER;
        if (typeStr == jellyfin::mediaTypeMovie) return domain::MediaType::MOVIE;
        if (typeStr == jellyfin::mediaTypeSeries) return domain::MediaType::SERIES;
        if (typeStr == jellyfin::mediaTypeSeason) return domain::MediaType::SEASON;
        if (typeStr == jellyfin::mediaTypeEpisode) return domain::MediaType::EPISODE;
        if (typeStr == jellyfin::mediaTypeAudio) return domain::MediaType::AUDIO;
        if (typeStr == jellyfin::mediaTypeMusicAlbum) return domain::MediaType::MUSIC_ALBUM;
        if (typeStr == jellyfin::mediaTypePlaylist) return domain::MediaType::PLAYLIST;
        if (typeStr == jellyfin::mediaTypeBook) return domain::MediaType::BOOK;
        if (typeStr == jellyfin::mediaTypeTvChannel) return domain::MediaType::LIVE_TV;
        return domain::MediaType::UNKNOWN;
    }

    static domain::MediaItemEntity toDomain(const jellyfin::Item& item) {
        domain::MediaItemEntity entity;
        entity.id = item.Id;
        entity.name = item.Name;
        entity.type = mapType(item.Type);
        entity.productionYear = static_cast<int>(item.ProductionYear);
        entity.runTimeTicks = item.RunTimeTicks;
        entity.playbackPositionTicks = item.UserData.PlaybackPositionTicks;
        entity.isFavorite = item.UserData.IsFavorite;
        entity.isPlayed = item.UserData.Played;

        auto itPrimary = item.ImageTags.find(jellyfin::imageTypePrimary);
        if (itPrimary != item.ImageTags.end()) {
            entity.primaryImageTag = itPrimary->second;
        }

        auto itThumb = item.ImageTags.find(jellyfin::imageTypeThumb);
        if (itThumb != item.ImageTags.end()) {
            entity.thumbImageTag = itThumb->second;
        }

        auto itBackdrop = item.ImageTags.find(jellyfin::imageTypeBackdrop);
        if (itBackdrop != item.ImageTags.end()) {
            entity.backdropImageTag = itBackdrop->second;
        }

        return entity;
    }
};

} // namespace data
