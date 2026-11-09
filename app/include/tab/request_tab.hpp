#pragma once

#include <view/auto_tab_frame.hpp>
#include "domain/models/request_item.hpp"
#include <vector>

class RecyclingGrid;

class RequestTab : public AttachedView {
public:
    RequestTab();
    ~RequestTab() override;

    void onCreate() override;
    static brls::View* create();

private:
    BRLS_BIND(brls::InputCell, inputSearch, "request/search/input");
    BRLS_BIND(brls::DetailCell, btnFilter, "request/search/filter");
    BRLS_BIND(brls::DetailCell, btnSettings, "request/search/settings");
    BRLS_BIND(brls::Label, labelStatus, "request/status/label");
    BRLS_BIND(RecyclingGrid, grid, "request/grid");

    enum class FilterType {
        ALL = 0,
        MOVIES,
        TV,
    };

    FilterType currentFilter = FilterType::ALL;
    std::string lastQuery;
    std::vector<domain::RequestMediaItem> currentResults;

    void cycleFilter();
    void doSearch(const std::string& query);
    void renderResults();
};
