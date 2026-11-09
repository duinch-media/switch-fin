#pragma once

#include <functional>
#include <string>
#include <borealis.hpp>
#include "domain/models/request_item.hpp"

class ArrActions {
public:
    static void handleMovieRequest(brls::View* parent, const std::string& title, int year);
    static void handleSeriesRequest(brls::View* parent, const std::string& title, int year);

    /// Shows the item's status, or asks to request it on Radarr (movie) / Sonarr (TV).
    /// `onDone` runs on the main thread after a successful request.
    static void confirmRequest(const domain::RequestMediaItem& item, std::function<void()> onDone = nullptr);
};
