#include "tab/request_tab.hpp"
#include "tab/arr_setting.hpp"
#include "data/arr_client.hpp"
#include "utils/arr_config.hpp"
#include "view/arr_source.hpp"
#include "view/recycling_grid.hpp"
#include "view/video_card.hpp"
#include <borealis/core/thread.hpp>
#include "telemetry/telemetry_client.hpp"

namespace {

const char* HINT_TEXT = "Enter a title above to search Radarr & Sonarr";

}  // namespace

brls::View* RequestTab::create() { return new RequestTab(); }

RequestTab::RequestTab() {
    this->inflateFromXMLRes("xml/tabs/request_tab.xml");
    telemetry::TelemetryClient::instance().recordScreenView("request");

    this->inputSearch->init(
        "Search Media", "",
        [this](std::string query) {
            if (!query.empty() && query != this->lastQuery) {
                this->doSearch(query);
            }
        },
        "Type movie or TV name...", "", 255);

    // DetailCell/InputCell expose no "detail"/"hint" XML attributes: set in code
    this->btnFilter->setDetailText("All");
    this->btnSettings->setDetailText("Radarr/Sonarr");

    this->btnFilter->registerClickAction([this](...) {
        this->cycleFilter();
        return true;
    });

    this->btnSettings->registerClickAction([this](...) {
        this->present(new ArrSetting());
        return true;
    });

    this->grid->registerCell("Cell", MediaCardCell::create);
    // RecyclingGrid starts on skeleton cards: show the hint until a search runs
    this->grid->setEmpty(HINT_TEXT, "icon/ico-search.svg");
}

RequestTab::~RequestTab() = default;

void RequestTab::onCreate() {}

void RequestTab::cycleFilter() {
    if (this->currentFilter == FilterType::ALL) {
        this->currentFilter = FilterType::MOVIES;
        this->btnFilter->setDetailText("Movies (Radarr)");
    } else if (this->currentFilter == FilterType::MOVIES) {
        this->currentFilter = FilterType::TV;
        this->btnFilter->setDetailText("TV (Sonarr)");
    } else {
        this->currentFilter = FilterType::ALL;
        this->btnFilter->setDetailText("All");
    }
    this->renderResults();
}

void RequestTab::doSearch(const std::string& query) {
    this->lastQuery = query;
    bool radarr = ArrConfig::isRadarrConfigured();
    bool sonarr = ArrConfig::isSonarrConfigured();
    if (!radarr && !sonarr) {
        this->labelStatus->setText("Radarr and Sonarr are not configured. Click Settings to configure.");
        return;
    }

    // both services are always queried: the filter only narrows what is shown,
    // so switching it never needs a new search
    this->labelStatus->setText("Searching for \"" + query + "\"...");
    this->grid->showSkeleton();

    ASYNC_RETAIN
    brls::async([ASYNC_TOKEN, query, radarr, sonarr]() {
        auto& client = data::ArrClient::instance();
        std::vector<domain::RequestMediaItem> results;
        std::string errors;
        auto collect = [&](const std::string& service, const auto& res) {
            if (res.isOk()) {
                results.insert(results.end(), res.value().begin(), res.value().end());
            } else {
                errors += (errors.empty() ? "" : " | ") + service + ": " + res.error().message;
            }
        };
        if (radarr) collect("Radarr", client.searchMovie(query));
        if (sonarr) collect("Sonarr", client.searchSeries(query));

        brls::sync([ASYNC_TOKEN, query, errors, results]() mutable {
            ASYNC_RELEASE
            this->currentResults = std::move(results);
            std::string status = this->currentResults.empty()
                                     ? "No results found for \"" + query + "\"."
                                     : "Found " + std::to_string(this->currentResults.size()) + " results for \"" +
                                           query + "\".";
            // an unreachable server must not look like "no results"
            if (!errors.empty()) status += "  Error: " + errors;
            this->labelStatus->setText(status);
            this->renderResults();
        });
    });
}

void RequestTab::renderResults() {
    std::vector<domain::RequestMediaItem> shown;
    for (const auto& item : this->currentResults) {
        if (this->currentFilter == FilterType::MOVIES && item.type != domain::RequestType::MOVIE) continue;
        if (this->currentFilter == FilterType::TV && item.type != domain::RequestType::TV) continue;
        shown.push_back(item);
    }
    if (shown.empty()) {
        this->grid->setEmpty(this->lastQuery.empty() ? HINT_TEXT : "No results", "icon/ico-search.svg");
        return;
    }

    // statuses change after a request: search again to refresh them
    auto refresh = [this]() {
        if (!this->lastQuery.empty()) this->doSearch(this->lastQuery);
    };
    bool mixed = this->currentFilter == FilterType::ALL;
    this->grid->setDataSource(new ArrDataSource(std::move(shown), refresh, mixed));
}
