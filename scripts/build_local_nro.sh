#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

echo "[build_local_nro] Building Switchfin.nro inside devkitpro/devkita64:latest..."
docker run --rm -v "$PWD:/workspace" -w /workspace devkitpro/devkita64:latest bash -c '
    export DEVKITPRO=/opt/devkitpro
    export PATH="$DEVKITPRO/tools/bin:$DEVKITPRO/devkitA64/bin:$PATH"
    git config --system --add safe.directory /workspace
    cmake -B build_switch \
        -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
        -DPLATFORM_SWITCH=ON \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build build_switch --target Switchfin.nro -j$(nproc)
    cp build_switch/Switchfin.nro ./Switchfin.nro
'

ls -lh Switchfin.nro
