# Switchfin — Continuous Learning & Mistake Ledger

> Read during Gate 1. Append 2-line prevention rules (`Symptom -> Root Cause & Rule`) whenever a subtle bug or user correction occurs.

- **Duplicate NRO deployment on Nintendo Switch**: Uploading `.nro` to both `/switch/App.nro` and `/switch/App/App.nro` causes duplicate entries in hbmenu and wastes 35+ MB of SD card space.
  - *Root Cause & Rule*: `/switch/<App>/` is reserved strictly for application data/configuration (`config.json`, fonts, caches, downloads). Always deploy the `.nro` binary to a single location (`/switch/Switchfin.nro`), never create a duplicate binary inside its data subdirectory.


- **Opening the Request (download-icon) tab crashed the app**: Atmosphère report showed a Data Abort in libnx `poll()` on an image worker thread, which hid the real cause. The main thread was in borealis `fatal("Unknown XML attribute…")` while inflating `request_tab.xml`. The fatal shut down sockets, and the worker's curl `poll` then dereferenced the removed `soc` devoptab.
  - *Root Cause & Rule*: borealis has no `hint`/`detail` XML attributes on `InputCell`/`DetailCell` and no `lines` on `Label`, and `@theme/<name>` must be registered in `config_theme.cpp`. Set those values in C++ (`init(...)`, `setDetailText`). When a crash report points at a worker thread, check the main thread's stack for `fatal` first.
