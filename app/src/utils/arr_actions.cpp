#include "utils/arr_actions.hpp"
#include "utils/arr_config.hpp"
#include "data/arr_client.hpp"
#include "utils/dialog.hpp"
#include <borealis/core/thread.hpp>

void ArrActions::handleMovieRequest(brls::View* parent, const std::string& title, int year) {
    if (!ArrConfig::isRadarrConfigured()) {
        Dialog::show("Radarr is not configured. Go to Settings -> Radarr/Sonarr to set it up.");
        return;
    }

    brls::Application::blockInputs();
    brls::async([title, year]() {
        auto res = data::ArrClient::instance().searchMovie(title);
        brls::sync([res, title, year]() {
            brls::Application::unblockInputs();
            if (!res.isOk()) {
                Dialog::show("Failed to query Radarr: " + res.error().message);
                return;
            }

            const auto& list = res.value();
            if (list.empty()) {
                Dialog::show("Movie not found in Radarr lookup for \"" + title + "\".");
                return;
            }

            domain::RequestMediaItem match = list.front();
            for (const auto& m : list) {
                if (year > 0 && m.year == year) {
                    match = m;
                    break;
                }
            }

            if (match.status == domain::RequestStatus::AVAILABLE) {
                Dialog::show(title + "\n\nStatus in Radarr: Available / Downloaded.");
            } else if (match.status == domain::RequestStatus::MONITORED) {
                Dialog::show(title + "\n\nStatus in Radarr: Monitored (Searching for downloads).");
            } else {
                std::string msg = "Request \"" + title + "\" on Radarr?\n\nRadarr will monitor and search for downloads.";
                auto* dialog = new brls::Dialog(msg);
                dialog->addButton("Cancel", []() {});
                dialog->addButton("Request on Radarr", [match]() {
                    brls::async([match]() {
                        auto r = data::ArrClient::instance().requestMovie(match, 0, "");
                        brls::sync([r, title = match.title]() {
                            if (r.isOk()) {
                                Dialog::show("Successfully requested \"" + title + "\" on Radarr!");
                            } else {
                                Dialog::show("Failed to request on Radarr: " + r.error().message);
                            }
                        });
                    });
                });
                dialog->open();
            }
        });
    });
}

void ArrActions::handleSeriesRequest(brls::View* parent, const std::string& title, int year) {
    if (!ArrConfig::isSonarrConfigured()) {
        Dialog::show("Sonarr is not configured. Go to Settings -> Radarr/Sonarr to set it up.");
        return;
    }

    brls::Application::blockInputs();
    brls::async([title, year]() {
        auto res = data::ArrClient::instance().searchSeries(title);
        brls::sync([res, title, year]() {
            brls::Application::unblockInputs();
            if (!res.isOk()) {
                Dialog::show("Failed to query Sonarr: " + res.error().message);
                return;
            }

            const auto& list = res.value();
            if (list.empty()) {
                Dialog::show("Series not found in Sonarr lookup for \"" + title + "\".");
                return;
            }

            domain::RequestMediaItem match = list.front();
            for (const auto& s : list) {
                if (year > 0 && s.year == year) {
                    match = s;
                    break;
                }
            }

            if (match.status == domain::RequestStatus::AVAILABLE) {
                Dialog::show(title + "\n\nStatus in Sonarr: Available / Downloaded.");
            } else if (match.status == domain::RequestStatus::MONITORED) {
                Dialog::show(title + "\n\nStatus in Sonarr: Monitored (Searching for downloads).");
            } else {
                std::string msg = "Request \"" + title + "\" on Sonarr?\n\nSonarr will monitor and search for missing episodes.";
                auto* dialog = new brls::Dialog(msg);
                dialog->addButton("Cancel", []() {});
                dialog->addButton("Request on Sonarr", [match]() {
                    brls::async([match]() {
                        auto r = data::ArrClient::instance().requestSeries(match, 0, "");
                        brls::sync([r, title = match.title]() {
                            if (r.isOk()) {
                                Dialog::show("Successfully requested \"" + title + "\" on Sonarr!");
                            } else {
                                Dialog::show("Failed to request on Sonarr: " + r.error().message);
                            }
                        });
                    });
                });
                dialog->open();
            }
        });
    });
}
