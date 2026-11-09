#include "view/arr_source.hpp"
#include "view/video_card.hpp"
#include "view/svg_image.hpp"
#include "utils/arr_actions.hpp"

namespace {

std::string statusText(domain::RequestStatus status) {
    switch (status) {
    case domain::RequestStatus::AVAILABLE:
        return "Available";
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

}  // namespace

ArrDataSource::ArrDataSource(
    std::vector<domain::RequestMediaItem> items, std::function<void()> onRequested, bool showType)
    : list(std::move(items)), onRequested(std::move(onRequested)), showType(showType) {}

size_t ArrDataSource::getItemCount() { return this->list.size(); }

RecyclingGridItem* ArrDataSource::cellForRow(RecyclingView* recycler, size_t index) {
    auto* cell = dynamic_cast<MediaCardCell*>(recycler->dequeueReusableCell("Cell"));
    const auto& item = this->list.at(index);
    cell->labelTitle->setText(item.title);

    std::string ext = statusText(item.status);
    if (this->showType) ext = std::string(item.type == domain::RequestType::MOVIE ? "Movie" : "TV") + " · " + ext;
    if (item.year > 0) ext = std::to_string(item.year) + " · " + ext;
    cell->labelExt->setText(ext);

    cell->badgeTopRight->setVisibility(brls::Visibility::GONE);
    if (!item.posterUrl.empty()) Image::with(cell->picture, item.posterUrl);
    return cell;
}

void ArrDataSource::onItemSelected(brls::Box* recycler, size_t index) {
    ArrActions::confirmRequest(this->list.at(index), this->onRequested);
}

void ArrDataSource::clearData() { this->list.clear(); }
