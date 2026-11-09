#include "utils/arr_actions.hpp"
#include "utils/arr_config.hpp"
#include "data/arr_client.hpp"
#include "utils/dialog.hpp"
#include <borealis/core/thread.hpp>

namespace {

using LookupResult = domain::Result<std::vector<domain::RequestMediaItem>>;

std::string serviceName(domain::RequestType type) {
    return type == domain::RequestType::MOVIE ? "Radarr" : "Sonarr";
}

/// Looks the title up on Radarr/Sonarr, picks the entry matching `year`
/// (first result otherwise) and hands it to confirmRequest.
void lookupAndConfirm(domain::RequestType type, const std::string& title, int year) {
    bool configured =
        type == domain::RequestType::MOVIE ? ArrConfig::isRadarrConfigured() : ArrConfig::isSonarrConfigured();
    std::string service = serviceName(type);
    if (!configured) {
        Dialog::show(service + " is not configured. Go to Settings -> Radarr/Sonarr to set it up.");
        return;
    }

    brls::Application::blockInputs();
    brls::async([type, title, year, service]() {
        auto& client = data::ArrClient::instance();
        LookupResult res = type == domain::RequestType::MOVIE ? client.searchMovie(title) : client.searchSeries(title);
        brls::sync([res, title, year, service]() {
            brls::Application::unblockInputs();
            if (!res.isOk()) {
                Dialog::show("Failed to query " + service + ": " + res.error().message);
                return;
            }
            const auto& list = res.value();
            if (list.empty()) {
                Dialog::show("\"" + title + "\" was not found in " + service + " lookup.");
                return;
            }
            const domain::RequestMediaItem* match = &list.front();
            for (const auto& m : list) {
                if (year > 0 && m.year == year) {
                    match = &m;
                    break;
                }
            }
            ArrActions::confirmRequest(*match);
        });
    });
}

}  // namespace

void ArrActions::handleMovieRequest(brls::View* parent, const std::string& title, int year) {
    lookupAndConfirm(domain::RequestType::MOVIE, title, year);
}

void ArrActions::handleSeriesRequest(brls::View* parent, const std::string& title, int year) {
    lookupAndConfirm(domain::RequestType::TV, title, year);
}

void ArrActions::confirmRequest(const domain::RequestMediaItem& item, std::function<void()> onDone) {
    std::string service = serviceName(item.type);
    std::string title = item.title + (item.year > 0 ? " (" + std::to_string(item.year) + ")" : "");
    std::string overview = item.overview.empty() ? "(No overview available)" : item.overview;
    if (overview.length() > 280) overview = overview.substr(0, 277) + "...";
    std::string header = title + "\n\n" + overview + "\n\n";

    if (item.status == domain::RequestStatus::AVAILABLE) {
        Dialog::show(header + "Status: already downloaded in " + service + ".");
        return;
    }
    if (item.status == domain::RequestStatus::MONITORED) {
        Dialog::show(header + "Status: monitored in " + service + " (searching for downloads).");
        return;
    }

    auto* dialog = new brls::Dialog(header + "Request and download via " + service + "?");
    dialog->addButton("Cancel", []() {});
    dialog->addButton("Request", [item, service, onDone]() {
        brls::async([item, service, onDone]() {
            auto& client = data::ArrClient::instance();
            auto res = item.type == domain::RequestType::MOVIE ? client.requestMovie(item, 0, "")
                                                               : client.requestSeries(item, 0, "");
            bool ok = res.isOk();
            std::string error = ok ? "" : res.error().message;
            brls::sync([ok, error, title = item.title, service, onDone]() {
                if (!ok) {
                    Dialog::show("Failed to request \"" + title + "\" on " + service + ": " + error);
                    return;
                }
                Dialog::show("\"" + title + "\" was added to " + service + " and a search was started.");
                if (onDone) onDone();
            });
        });
    });
    dialog->open();
}
