#include "view/arr_search_list.hpp"
#include "view/h_recycling.hpp"
#include "view/video_card.hpp"
#include "view/arr_source.hpp"
#include "data/arr_client.hpp"
#include "utils/arr_config.hpp"
#include <borealis/core/thread.hpp>

using RequestItems = std::vector<domain::RequestMediaItem>;

ArrSearchList::ArrSearchList() {
    brls::Logger::debug("View ArrSearchList: create");
    this->inflateFromXMLRes("xml/view/recycler_list.xml");

    this->registerStringXMLAttribute("title", [this](std::string value) { this->title->setTitle(value); });
    this->registerStringXMLAttribute("service", [this](std::string value) { this->isMovie = value != "sonarr"; });

    this->recycler->registerCell("Cell", MediaCardCell::create);
}

ArrSearchList::~ArrSearchList() {
    *this->alive = false;
    brls::Logger::debug("View ArrSearchList: delete");
}

void ArrSearchList::doRequest(const std::string& searchTerm) {
    bool configured = this->isMovie ? ArrConfig::isRadarrConfigured() : ArrConfig::isSonarrConfigured();
    if (!configured || searchTerm.empty()) {
        this->setVisibility(brls::Visibility::GONE);
        return;
    }
    this->lastTerm = searchTerm;
    bool movie = this->isMovie;

    ASYNC_RETAIN
    brls::async([ASYNC_TOKEN, searchTerm, movie]() {
        auto& client = data::ArrClient::instance();
        auto res = movie ? client.searchMovie(searchTerm) : client.searchSeries(searchTerm);
        bool ok = res.isOk();
        std::string error = ok ? "" : res.error().message;
        RequestItems items;
        if (ok) {
            // titles already downloaded are listed by the Jellyfin rows
            for (const auto& it : res.value())
                if (it.status != domain::RequestStatus::AVAILABLE) items.push_back(it);
        }
        brls::sync([ASYNC_TOKEN, ok, error, items]() mutable {
            ASYNC_RELEASE
            if (!ok) {
                this->title->setSubtitle(error);
                this->recycler->clearData();
                return;
            }
            if (items.empty()) {
                this->setVisibility(brls::Visibility::GONE);
                this->recycler->clearData();
                return;
            }
            this->setVisibility(brls::Visibility::VISIBLE);
            this->title->setSubtitle(std::to_string(items.size()));
            // refresh statuses after a successful request
            auto refresh = [this, alive = this->alive]() {
                if (*alive) this->doRequest(this->lastTerm);
            };
            this->recycler->setDataSource(new ArrDataSource(std::move(items), refresh));
        });
    });
}

brls::View* ArrSearchList::create() { return new ArrSearchList(); }
