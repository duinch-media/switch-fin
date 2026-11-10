#!/usr/bin/env bash
# Build and run the host-side telemetry tests (no devkitPro, borealis or mpv needed).
set -euo pipefail
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/switchfin_telemetry_test"
${CXX:-clang++} -std=c++17 -Wall -Wextra -Iapp/include -Ilibrary/borealis/library/include/borealis/extern \
  tests/telemetry/test_health.cpp \
  app/src/telemetry/health_queue.cpp app/src/telemetry/health_uploader.cpp \
  app/src/telemetry/health_config.cpp app/src/telemetry/health_snapshot.cpp -o "$out"
"$out"
