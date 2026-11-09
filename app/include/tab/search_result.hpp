/*
    Copyright 2023 dragonflylee
*/

#pragma once

#include <borealis.hpp>

class SearchList;
class ArrSearchList;

class SearchResult : public brls::Box {
public:
    SearchResult(const std::string& searchTerm);
    ~SearchResult() override;

    static brls::View* create();

private:
    BRLS_BIND(SearchList, searchMovie, "search/movie");
    BRLS_BIND(SearchList, searchSeries, "search/series");
    BRLS_BIND(SearchList, searchEpisode, "search/episode");
    BRLS_BIND(SearchList, searchMusic, "search/music");
    BRLS_BIND(ArrSearchList, searchRadarr, "search/radarr");
    BRLS_BIND(ArrSearchList, searchSonarr, "search/sonarr");
};