# Active Execution Plan — Combined Jellyfin + Radarr + Sonarr Search

## 1. Objective
The Search tab result screen (`SearchResult`) queries only Jellyfin, so media missing from the
library cannot be found or requested. Query Jellyfin, Radarr and Sonarr at the same time and let
the user request a missing title straight from a result card.

## 2. Design
- New presentation view `ArrSearchList` (`app/include/view/arr_search_list.hpp`,
  `app/src/view/arr_search_list.cpp`): same layout as `SearchList` (`xml/view/recycler_list.xml`,
  Header + `HRecyclerFrame`, `MediaCardCell`). XML attrs `title`, `service` (`radarr` | `sonarr`).
  - `doRequest(term)`: hides the row if the service is not configured; otherwise async
    `ArrClient::searchMovie/searchSeries` with the ASYNC_RETAIN/ASYNC_TOKEN lifetime pattern.
  - Drops `AVAILABLE` items (already shown by the Jellyfin rows); hides the row when empty;
    shows the error in the header subtitle on failure.
  - Card: title, `year · status`, poster via `Image::with`.
  - Select → shared `ArrActions::confirmRequest(item)` dialog.
- `ArrActions::confirmRequest(item, onDone)`: one status/confirm/request dialog for movies and TV,
  reused by `handleMovieRequest`, `handleSeriesRequest` and `RequestTab::showRequestDialog`
  (removes three copies of the same flow).
- `search_result.xml`: two `ArrSearchList` rows after Series; `SearchResult` calls `doRequest`
  on them, so the 3 APIs run in parallel.

## 3. Related fixes
- `ArrConfig::parseJsonData`: accept `rootFolder` / `qualityProfileId` in the nested
  `radarr` / `sonarr` objects (what `arr_config.json` uses). Before, Sonarr fell back to `/tv`
  while the server root is `/series`, so TV requests failed.
- `ArrClient` poster URLs: TMDB `/t/p/original/` → `/t/p/w342/`; TVDB v4 `x.jpg` → `x_t.jpg`
  (both verified live: ~1 MB → ~44 KB).

## 4. Verification
- Static: XML attribute/theme validator over `resources/xml`, LOC caps (<= 450).
- No local devkitPro: on-device test pending (CI build + `scripts/deploy_switch.py`).
