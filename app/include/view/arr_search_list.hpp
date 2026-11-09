#pragma once

#include <memory>
#include <borealis.hpp>

class HRecyclerFrame;

/// Search row backed by Radarr (movies) or Sonarr (TV): lists titles that
/// are not in the library yet and lets the user request them.
/// XML: <ArrSearchList title="..." service="radarr|sonarr" />
class ArrSearchList : public brls::Box {
public:
    ArrSearchList();
    ~ArrSearchList() override;

    void doRequest(const std::string& searchTerm);

    static brls::View* create();

private:
    BRLS_BIND(brls::Header, title, "recycler/title");
    BRLS_BIND(HRecyclerFrame, recycler, "recycler/videos");

    bool isMovie = true;
    std::string lastTerm;
    /// cleared in the destructor: a request dialog may finish after the row is gone
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};
