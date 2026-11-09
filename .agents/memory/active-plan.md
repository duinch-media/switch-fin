# Active Execution Plan — Player Health Telemetry (client side)

## 1. Objective
Measure switchfin player health and report it to the shared `app-health` server (separate repo
`/Volumes/512GB/AI Workspace/app-health`, contract `app-health/1`, endpoint `POST /v1/events`).
switchfin is the first client. The server is generic: nothing switchfin-specific belongs there.

Rules agreed with the user:
- While playing: aggregate in RAM, persist a snapshot to a local file. **No network traffic.**
- Submit only when the user **leaves the player** or **exits the app**; retry the unsent queue on
  next launch.
- Network may be down or off: recording must never depend on it.

## 2. Metrics (`playback_session`)
| Metric | Source | Hook |
|---|---|---|
| `init_ms` | `steady_clock` around `mpv_create` → `mpv_initialize` → `mpv_render_context_create` | `mpv_core.cpp:103/183/280` |
| `api_ms` | Jellyfin `PlaybackInfo` request (curl `CURLINFO_*_TIME_T`: DNS, connect, TLS, TTFB, total) | `http.cpp:221`, `player_view.cpp:373` |
| `open_ms` | `loadfile` sent → `START_FILE` → `FILE_LOADED` | `MPVCore::setUrl`, `mpv_event.cpp` |
| `ttff_ms` | `loadfile` → first `MPV_RENDER_UPDATE_FRAME` after `FILE_LOADED` (render thread: store an atomic timestamp only) | `mpv_render.cpp:77` |
| `stall_count`, `stall_ms`, `rebuffer_ratio` | observe `paused-for-cache` (+ `cache-buffering-state`); split startup / seek / mid-play stalls; ratio = mid-play stall time / watch time | new observers (ids 20+) |
| `seek_count`, `seek_p50_ms` | `seek` command → `PLAYBACK_RESTART` | `MPVCore::seek` |
| `input_kbps_avg`, `input_kbps_p10` | `demuxer-cache-state/raw-input-rate`, sampled at 1 Hz only while `demuxer-cache-idle == false` (not `cache-speed`: it drops to 0 when the cache is full) | main-thread `RepeatingTimer` |
| `dropped_frames` | `decoder-frame-drop-count` + `frame-drop-count` (+ `vo-delayed-frame-count`) | read once at session end |

Context: `play_method`, `video_codec`, `hwdec` (`hwdec-current`), `resolution`, `bitrate_kbps`,
`wifi_strength` (`nifmGetInternetConnectionStatus`), `docked` (`apmGetPerformanceMode`).
Phase 3 adds: used memory (`svcGetInfo`), SoC temperature (`tsGetTemperature`), server transcode fps
(Jellyfin Sessions `TranscodingInfo.Framerate`, already fetched by `VideoProfile::onRequest`).

`end_reason`: `user_exit | eof | error | app_exit | crash`.

## 3. Design (Inside-Out)
- **Domain** (pure C++):
  - `domain/models/health_event.hpp`: `HealthEvent` envelope (`schema`, `event`, `app`,
    `app_version`, `platform`, `device_id`, `session_id`, `ts_start/ts_end`, `end_reason`,
    `metrics` map<string,double>, `context` map<string,string>).
  - `domain/repositories/health_reporter.hpp`: port `enqueue(HealthEvent)`, `flush(deadline)`.
  - `domain/models/playback_stats.hpp`: pure aggregator (stall/seek timers, rate samples, p10/p50),
    no mpv types, so it can be unit-tested on the host.
- **Data**:
  - `data/health_queue.cpp`: `perf/pending.jsonl` append, 256 KB cap (drop oldest), atomic rewrite
    after an upload succeeds. `perf/current.json` is the in-progress snapshot (every ~30 s).
  - `data/health_uploader.cpp`: batch `POST /v1/events` with `Authorization: Bearer <key>`,
    short timeout, keep the queue on any failure.
  - Config (`health` section in a local JSON next to `arr_config.json`): `enabled`, `endpoint`,
    `apiKey`. No defaults in code. Missing endpoint → record locally only.
  - `device_id`: random UUID created once and stored in the config dir (never a hardware serial).
- **State / collector**: `utils/playback_metrics.cpp` subscribes to `mpvCoreEvent` plus the new
  observers, drives a state machine `OPENING → LOADED → FIRST_FRAME → PLAYING ⇄ STALLED/SEEKING →
  ENDED/ERROR`, and feeds `PlaybackStats`.
- **Lifecycle hooks**:
  - Leaving the player (`PlayerView` teardown): finalise → enqueue → async flush.
  - App exit: synchronous flush with a ~2 s deadline **before** `userAppExit`/`socketExit`
    (lesson from the a13732c crash: sockets die on exit).
  - Home button / focus lost: persist the snapshot only, no network.
  - Next launch: if `current.json` exists without an end marker, enqueue it as `end_reason=crash`,
    then flush the queue in the background.
- **Security**: strip `api_key=` and tokens from every string (the mpv `path` contains Jellyfin's
  `api_key`); never log the gateway key.

## 4. Phases
1. **P1 — local only**: domain models + `PlaybackStats`, collector + observers, first-frame hook,
   `api_ms`, file queue, crash marker. Host unit tests for `PlaybackStats` and the queue
   (new pure-C++ ctest target).
2. **P2 — submit**: uploader + config + leave/exit/next-launch flush, once the `app-health`
   gateway contract is final.
3. **P3 — insight**: "QoE" section in `VideoProfile`, Switch context (memory, temperature),
   server transcode fps.

## 5. Verification
- ctest for the pure aggregator/queue; LOC caps (<= 300 warn, <= 450 fail); XML validator if
  views change.
- No local devkitPro: on-device check through a CI build + `scripts/deploy_switch.py`, then pull
  `perf/*.jsonl` over FTP and compare with the server dashboard.
