#pragma once

#include <string>
#include <borealis.hpp>

class ArrActions {
public:
    static void handleMovieRequest(brls::View* parent, const std::string& title, int year);
    static void handleSeriesRequest(brls::View* parent, const std::string& title, int year);
};
