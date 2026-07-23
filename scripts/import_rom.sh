#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
rom="${1:-$repo_root/../native-gb-pokemon-red/roms/pokemon_red.gb}"
output="${2:-$repo_root/data/runtime/imports/pokemon_red_us_rev_0}"

cd "$repo_root"
if [[ ! -f build-debug/build.ninja ]]; then
    cmake --preset dev
fi
cmake --build --preset dev --target native-gb-pokemon-red-modern-import
exec "$repo_root/build-debug/native-gb-pokemon-red-modern-import" "$rom" "$output"
