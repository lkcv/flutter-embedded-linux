#!/usr/bin/env bash
# Build the Sony eLinux Wayland example runner with Kurage's required options.
# Run from any directory, inside the configured Linux build environment.
set -euo pipefail

SOURCE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SOURCE"

if [[ ! -f build/libflutter_engine.so ]]; then
  echo 'Missing build/libflutter_engine.so: provide the matching release Flutter eLinux engine first.' >&2
  exit 1
fi

cmake -S . -B build \
  -DUSER_PROJECT_PATH=examples/flutter-wayland-client \
  -DFLUTTER_RELEASE=ON \
  -DUSE_GLES3=ON \
  -DENABLE_VSYNC=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "${JOBS:-$(nproc)}"

printf '\nBuilt %s/build/flutter-client\n' "$SOURCE"
