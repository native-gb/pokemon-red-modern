#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
preset="${POKERED_MODERN_PRESET:-dev}"
case "$preset" in
    dev) build_dir="build-debug" ;;
    release) build_dir="build-release" ;;
    asan) build_dir="build-asan" ;;
    *)
        echo "Unknown preset: $preset" >&2
        exit 2
        ;;
esac

"$repo_root/scripts/build.sh" "$preset"

import_root="$repo_root/data/runtime/imports/pokemon_red_us_rev_0"
world_cache="$import_root/compiled/world_maps.bin"
interaction_cache="$import_root/compiled/world_interactions.bin"
world_magic="$(head -c 4 "$world_cache" 2>/dev/null || true)"
interaction_magic="$(head -c 4 "$interaction_cache" 2>/dev/null || true)"
if [[ "$world_magic" != "PMV9" || "$interaction_magic" != "PWI1" ]]; then
    rom="$repo_root/../native-gb-pokemon-red/roms/pokemon_red.gb"
    if [[ ! -f "$rom" ]]; then
        echo "Imported runtime data is stale and the canonical ROM is missing: $rom" >&2
        exit 1
    fi
    "$repo_root/$build_dir/native-gb-pokemon-red-modern-import" "$rom" "$import_root"
fi

exec "$repo_root/$build_dir/native-gb-pokemon-red-modern" "$@"
