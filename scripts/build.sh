#!/usr/bin/env bash
set -euo pipefail

preset="${1:-dev}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
cd "$repo_root"

case "$preset" in
    dev) build_dir="build-debug" ;;
    release) build_dir="build-release" ;;
    asan) build_dir="build-asan" ;;
    *)
        echo "Unknown preset: $preset" >&2
        exit 2
        ;;
esac

# A configure pass against fetched SDL/Gubsy dependencies can refresh generated
# files even when this project did not change. Configure only a missing tree;
# Ninja will rerun CMake itself whenever an actual CMake input is newer.
if [[ ! -f "$build_dir/build.ninja" ]]; then
    cmake --preset "$preset"
fi
cmake --build --preset "$preset" --target native-gb-pokemon-red-modern
