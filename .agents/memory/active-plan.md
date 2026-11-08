# Active Plan: Radarr & Sonarr Integration for Switchfin

## Goals
Support Radarr and Sonarr integration to search and request media not yet in the Jellyfin library:
1. Search screen to search for movies/series outside Jellyfin via Radarr & Sonarr lookup APIs.
2. Radarr/Sonarr action button on Movie and Series detail screens.
3. Configuration for Radarr and Sonarr (URL, API Key, Root Folder, Quality Profile) with connection testing.

## Architectural Design (Modular Clean)

### 1. Domain Layer (Pure C++)
- `app/include/domain/models/request_item.hpp`: Pure C++ entity for external media item (`RequestMediaItem`, `RequestType`: MOVIE / TV, `RequestStatus`).
- `app/include/domain/repositories/arr_repository.hpp`: Pure interface `IArrRepository` for search, request, and status queries.

### 2. Data / Infrastructure Layer
- `app/include/utils/arr_config.hpp` & `app/src/utils/arr_config.cpp`: Storage of Radarr and Sonarr settings in AppConfig.
- `app/include/data/arr_client.hpp` & `app/src/data/arr_client.cpp`: Implementation of Radarr & Sonarr v3 HTTP API calls via `HTTP` client.

### 3. Presentation Layer
- `app/include/tab/arr_setting.hpp` & `app/src/tab/arr_setting.cpp`: Radarr & Sonarr settings view.
- `app/include/tab/request_tab.hpp` & `app/src/tab/request_tab.cpp`: Search screen for searching media via Radarr & Sonarr lookup, displaying results with poster, title, year, type badge, and request action.
- Update `media_movie.hpp`, `media_movie.cpp`, `resources/xml/tabs/movie.xml`: Add Radarr button.
- Update `media_series.hpp`, `media_series.cpp`, `resources/xml/tabs/series.xml`: Add Sonarr button.
- Update `resources/xml/activity/main.xml` & `setting_tab`: Add Request Tab and Settings entry.

## Guardrails & Verification
- Anti-God-File cap: All new files <= 300 LOC (never > 450 LOC).
- Staged audit verification: `python3 project_pro.py audit --staged --fail-on-error`.
- Conventional Commits via `.githooks/commit-msg`.
