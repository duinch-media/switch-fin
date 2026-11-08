# Switchfin — Continuous Learning & Mistake Ledger

> Read during Gate 1. Append 2-line prevention rules (`Symptom -> Root Cause & Rule`) whenever a subtle bug or user correction occurs.

- **Duplicate NRO deployment on Nintendo Switch**: Uploading `.nro` to both `/switch/App.nro` and `/switch/App/App.nro` causes duplicate entries in hbmenu and wastes 35+ MB of SD card space.
  - *Root Cause & Rule*: `/switch/<App>/` is reserved strictly for application data/configuration (`config.json`, fonts, caches, downloads). Always deploy the `.nro` binary to a single location (`/switch/Switchfin.nro`), never create a duplicate binary inside its data subdirectory.


