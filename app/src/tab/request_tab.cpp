#include "tab/request_tab.hpp"
#include "tab/arr_setting.hpp"
#include "data/arr_client.hpp"
#include "utils/arr_config.hpp"
#include "utils/image.hpp"
#include "utils/arr_actions.hpp"
#include <borealis/core/thread.hpp>

class RequestCardItem : public brls::Box {
public:
    RequestCardItem(const domain::RequestMediaItem& item, std::function<void()> onClick) {
        this->inflateFromXMLRes("xml/view/request_card.xml");
        this->setFocusable(true);

        this->labelTitle->setText(item.title);
        this->labelYear->setText(item.year > 0 ? std::to_string(item.year) : "");
        this->labelType->setText(item.type == domain::RequestType::MOVIE ? "MOVIE" : "TV");

        if (item.status == domain::RequestStatus::AVAILABLE) {
            this->labelStatus->setText("Available");
            this->labelStatus->setTextColor(nvgRGB(46, 204, 113));
        } else if (item.status == domain::RequestStatus::MONITORED) {
            this->labelStatus->setText("Monitored");
            this->labelStatus->setTextColor(nvgRGB(52, 152, 219));
        } else if (item.status == domain::RequestStatus::REQUESTED) {
            this->labelStatus->setText("Requested");
            this->labelStatus->setTextColor(nvgRGB(241, 196, 15));
        } else {
            this->labelStatus->setText("Not Added");
            this->labelStatus->setTextColor(nvgRGB(149, 165, 166));
        }

        if (!item.posterUrl.empty()) {
            Image::with(this->imagePoster, item.posterUrl);
        }

        this->registerClickAction([onClick](...) {
            if (onClick) onClick();
            return true;
        });
    }

    ~RequestCardItem() override {
        Image::cancel(this->imagePoster);
    }

private:
    BRLS_BIND(brls::Image, imagePoster, "request/card/poster");
    BRLS_BIND(brls::Label, labelType, "request/card/type");
    BRLS_BIND(brls::Label, labelStatus, "request/card/status");
    BRLS_BIND(brls::Label, labelTitle, "request/card/title");
    BRLS_BIND(brls::Label, labelYear, "request/card/year");
};

brls::View* RequestTab::create() {
    return new RequestTab();
}

RequestTab::RequestTab() {
    this->inflateFromXMLRes("xml/tabs/request_tab.xml");

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
}

RequestTab::~RequestTab() = default;

void RequestTab::onCreate() {
}

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
    bool searchRadarr = (this->currentFilter == FilterType::ALL || this->currentFilter == FilterType::MOVIES);
    bool searchSonarr = (this->currentFilter == FilterType::ALL || this->currentFilter == FilterType::TV);

    if (searchRadarr && !ArrConfig::isRadarrConfigured() && !searchSonarr) {
        this->labelStatus->setText("Radarr is not configured. Click Settings to configure.");
        return;
    }
    if (searchSonarr && !ArrConfig::isSonarrConfigured() && !searchRadarr) {
        this->labelStatus->setText("Sonarr is not configured. Click Settings to configure.");
        return;
    }

    this->labelStatus->setText("Searching Radarr & Sonarr for \"" + query + "\"...");
    this->gridContainer->clearViews();

    brls::async([this, query, searchRadarr, searchSonarr]() {
        std::vector<domain::RequestMediaItem> allResults;

        if (searchRadarr && ArrConfig::isRadarrConfigured()) {
            auto mRes = data::ArrClient::instance().searchMovie(query);
            if (mRes.isOk()) {
                const auto& movies = mRes.value();
                allResults.insert(allResults.end(), movies.begin(), movies.end());
            }
        }

        if (searchSonarr && ArrConfig::isSonarrConfigured()) {
            auto sRes = data::ArrClient::instance().searchSeries(query);
            if (sRes.isOk()) {
                const auto& series = sRes.value();
                allResults.insert(allResults.end(), series.begin(), series.end());
            }
        }

        brls::sync([this, query, results = std::move(allResults)]() mutable {
            this->currentResults = std::move(results);
            if (this->currentResults.empty()) {
                this->labelStatus->setText("No results found for \"" + query + "\".");
            } else {
                this->labelStatus->setText(
                    "Found " + std::to_string(this->currentResults.size()) + " results for \"" + query + "\".");
            }
            this->renderResults();
        });
    });
}

void RequestTab::renderResults() {
    this->gridContainer->clearViews();
    for (size_t i = 0; i < this->currentResults.size(); ++i) {
        const auto& item = this->currentResults[i];
        if (this->currentFilter == FilterType::MOVIES && item.type != domain::RequestType::MOVIE) continue;
        if (this->currentFilter == FilterType::TV && item.type != domain::RequestType::TV) continue;

        auto* card = new RequestCardItem(item, [this, item]() {
            this->showRequestDialog(item);
        });
        this->gridContainer->addView(card);
    }
}

void RequestTab::showRequestDialog(const domain::RequestMediaItem& item) {
    ArrActions::confirmRequest(item, [this]() {
        if (!this->lastQuery.empty()) this->doSearch(this->lastQuery);
    });
}
