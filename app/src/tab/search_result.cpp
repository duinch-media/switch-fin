#include "tab/search_result.hpp"
#include "view/search_list.hpp"
#include "view/arr_search_list.hpp"

SearchResult::SearchResult(const std::string& searchTerm) {
    brls::Logger::debug("Tab SearchResult: create");
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/search_result.xml");

    this->searchMovie->doRequest(searchTerm);
    this->searchSeries->doRequest(searchTerm);
    this->searchEpisode->doRequest(searchTerm);
    this->searchMusic->doRequest(searchTerm);
    // Radarr/Sonarr run in parallel with the Jellyfin rows above
    this->searchRadarr->doRequest(searchTerm);
    this->searchSonarr->doRequest(searchTerm);
}

SearchResult::~SearchResult() { brls::Logger::debug("Tab SearchResult: delete"); }