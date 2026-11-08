# Active Execution Plan — Platform-Adaptive MPV Buffering & God-File Split

## 1. Objective
Fix playback buffering stalls/thrashing across all supported platforms by implementing intelligent platform-aware cache & stream buffer tuning in `AppConfig` and `MPVCore`, while splitting legacy God files (`config.cpp` and `mpv_core.cpp`) to comply with the `project-pro` file-length boundary (`<= 450 LOC` hard cap, target `<= 350 LOC`).

## 2. Root Cause Summary
1. `AppConfig::init()` defaulted `PLAYER_INMEMORY_CACHE` to `10` (10 MiB) on all platforms.
2. `MPVCore::init()` split `INMEMORY_CACHE` 50/50 between `demuxer-max-bytes` (10 MiB) and `demuxer-max-back-bytes` (5 MiB), leaving only ~5 MiB (~1.5–2.5s of 1080p stream) for forward readahead once backward cache filled.
3. Missing `cache-pause-wait` caused mpv to unpause after 1 second of buffering, triggering pause/resume thrashing under network jitter.
4. `demuxer-readahead-secs` was 20s on Switch and 1s (mpv default) on other platforms.
5. Missing `stream-buffer-size` and FFmpeg HTTP reconnect options (`reconnect=1,reconnect_streamed=1,reconnect_delay_max=5`).

## 3. Platform-Adaptive Buffering Matrix
| Platform | Default Cache | Forward / Back Split | `demuxer-readahead-secs` | `cache-pause-wait` | `stream-buffer-size` | Special Guards |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **PS Vita / TrimUI** (`__PSV__`, `TRIMUI`) | `20 MB` | 80% / 20% (max 8 MiB back) | `30s` | `3s` | `512KiB` | Low-RAM cap |
| **Nintendo Switch** (`__SWITCH__`) | `100 MB` | 80% / 20% (max 25 MiB back) | `60s` | `4s` | `2MiB` | Applet Mode (`!isApplicationMode()`) clamps to `<= 50 MB`; upgrades legacy `10 MB` default |
| **PS4 / Android** (`__PS4__`, `ANDROID`) | `100 MB` | 80% / 20% (max 32 MiB back) | `90s` | `3s` | `2MiB` | HTTP reconnect enabled |
| **Desktop** (`__APPLE__`, `__linux__`, `_WIN32`) | `200 MB` | 80% / 20% (max 50 MiB back) | `120s` | `3s` | `4MiB` | HTTP reconnect enabled |

## 4. Planned File Changes
- `app/src/utils/config.cpp`: Core `AppConfig::init()` (with platform-adaptive default cache) & `AppConfig::save()` (`<= 300 LOC`).
- `app/src/utils/config_meta.cpp`: `AppConfig::settingMap`, `generateDeviceId()`, path helpers, option/value index lookups (`<= 280 LOC`).
- `app/src/utils/config_auth.cpp`: Server, user, auth token, and remote storage methods (`<= 240 LOC`).
- `app/src/utils/config_theme.cpp`: Theme colors and resolution style metrics (`<= 130 LOC`).
- `app/src/view/mpv_core.cpp`: MPV lifecycle and platform-smart buffering configuration (`<= 360 LOC`).
- `app/src/view/mpv_render.cpp`: Framebuffer sizing, rendering, codec inspection (`<= 220 LOC`).
- `app/src/view/mpv_event.cpp`: MPV event loop, playback controls, and property accessors (`<= 280 LOC`).
