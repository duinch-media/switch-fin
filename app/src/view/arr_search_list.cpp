#include "view/arr_search_list.hpp"
#include "view/h_recycling.hpp"
#include "view/video_card.hpp"
#include "view/svg_image.hpp"
#include "data/arr_client.hpp"
#include "utils/arr_actions.hpp"
#include "utils/arr_config.hpp"
#include <borealis/core/thread.hpp>

namespace {

using RequestItems = std::vector<domain::RequestMediaItem>;

std::string statusText(domain::RequestStatus status) {
    switch (status) {
    case domain::RequestStatus::REQUESTED:
        return "Requested";
    case domain::RequestStatus::MONITORED:
        return "Monitored";
    case domain::RequestStatus::DOWNLOADING:
        return "Downloading";
    default:
        return "Not in library";
    }
}

class ArrDataSource : public RecyclingGridDataSource {
public:
    ArrDataSource(RequestItems items, std::function<void()> onRequested)
        : list(std::move(items)), onRequested(std::move(onRequested)) {}

    size_t getItemCount() override { return this->list.size(); }

    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override {
        auto* cell = dynamic_cast<MediaCardCell*>(recycler->dequeueReusableCell("Cell"));
        const auto& item = this->list.at(index);
        cell->labelTitle->setText(item.title);
        std::string ext = statusText(item.status);
        if (item.year > 0) ext = std::to_string(item.year) + " · " + ext;
        cell->labelExt->setText(ext);
        cell->badgeTopRight->setVisibility(brls::Visibility::GONE);
        if (!item.posterUrl.empty()) Image::with(cell->picture, item.posterUrl);
        return cell;
    }

    void onItemSelected(brls::Box* recycler, size_t index) override {
        ArrActions::confirmRequest(this->list.at(index), this->onRequested);
    }

    void clearData() override { this->list.clear(); }

private:
    RequestItems list;
    std::function<void()> onRequested;
};

}  // namespace

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
