#pragma once

#include <functional>
#include <vector>
#include <view/recycling_grid.hpp>
#include "domain/models/request_item.hpp"

/// Radarr/Sonarr lookup results as MediaCardCell posters (register the cell
/// as "Cell"). Selecting a card opens ArrActions::confirmRequest.
class ArrDataSource : public RecyclingGridDataSource {
public:
    /// `onRequested` runs on the main thread after a successful request;
    /// `showType` adds "Movie"/"TV" to the subtitle (lists mixing both).
    ArrDataSource(std::vector<domain::RequestMediaItem> items, std::function<void()> onRequested, bool showType = false);

    size_t getItemCount() override;
    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override;
    void onItemSelected(brls::Box* recycler, size_t index) override;
    void clearData() override;

private:
    std::vector<domain::RequestMediaItem> list;
    std::function<void()> onRequested;
    bool showType;
};
