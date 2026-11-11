# Active Execution Plan — Double-Tap Left/Right Seek (-5s / +30s)

## 1. Objective
Add double-tap left/right screen gesture in `VideoView` player to seek backward `-5s` (left) and forward `+30s` (right), while keeping center double-tap for play/pause and splitting `app/src/view/video_view.cpp` (924 LOC) to comply with the `<= 450 LOC` file size limit.

## 2. Design & File Boundaries
1. `app/src/utils/gesture.cpp` (< 210 LOC):
   - Separate `DOUBLE_TAP_TIME_MS` (`280` ms) from `LONG_TIME_MS` (`200` ms) and validate touch proximity (`MAX_DOUBLE_TAP_DISTANCE = 180` px) so double-taps and chained multi-taps on the same side register reliably without cross-screen false positives.
2. `app/include/view/video_view.hpp` (< 180 LOC):
   - Declare `registerTouchGestures()` helper on `VideoView`.
3. Split `app/src/view/video_view.cpp` (was 924 LOC) into 3 focused translation units (`<= 350 LOC` each):
   - `app/src/view/video_view.cpp`: Core lifecycle (`VideoView()`, `~VideoView()`), `draw()`, `invalidate()`, focus & controller button processing.
   - `app/src/view/video_view_controls.cpp`: `registerTouchGestures()` (including left `-5s` / right `+30s` / center `togglePlay()` double-tap handling), `requestSeeking()`, `requestVolume()`, `requestBrightness()`, `registerMpvEvent()`, `unRegisterMpvEvent()`, `registerActions()`.
   - `app/src/view/video_view_osd.cpp`: OSD state & UI controls (`toggleOSD`, `showOSD`, `hideOSD`, `toggleOSDLock`, `toggleSpeed`, `toggleVolume`, `setChapters`, `setList`, `setDanmakuEnable`, `close`, etc.).

## 3. Verification
- Re-run CMake in `build-switch` container to pick up new `.cpp` files, then compile `Switchfin.nro`.
- Run host unit tests (`./scripts/test_telemetry.sh`).
- Verify all modified/new files are `<= 450 LOC` and commit via `.githooks/pre-commit`.
- Deploy `Switchfin.nro` to Nintendo Switch (`ftp://192.168.1.17:5000/switch/Switchfin.nro`).
