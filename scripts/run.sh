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
rule_cache="$import_root/compiled/pokemon_rules.bin"
battle_rule_cache="$import_root/compiled/battle_rules.bin"
encounter_cache="$import_root/compiled/encounters.bin"
trainer_cache="$import_root/compiled/trainers.bin"
boot_cache="$import_root/compiled/boot_content.bin"
campaign_cache="$import_root/compiled/campaign_programs.bin"
audio_cache="$import_root/compiled/audio_content.bin"
world_magic="$(head -c 4 "$world_cache" 2>/dev/null || true)"
interaction_magic="$(head -c 4 "$interaction_cache" 2>/dev/null || true)"
rule_magic="$(head -c 4 "$rule_cache" 2>/dev/null || true)"
battle_rule_magic="$(head -c 4 "$battle_rule_cache" 2>/dev/null || true)"
encounter_magic="$(head -c 4 "$encounter_cache" 2>/dev/null || true)"
trainer_magic="$(head -c 4 "$trainer_cache" 2>/dev/null || true)"
boot_magic="$(head -c 4 "$boot_cache" 2>/dev/null || true)"
campaign_magic="$(head -c 4 "$campaign_cache" 2>/dev/null || true)"
audio_magic="$(head -c 4 "$audio_cache" 2>/dev/null || true)"
if [[ "$world_magic" != "PMVB" || "$interaction_magic" != "PWI3" ||
      "$rule_magic" != "PRL1" || "$battle_rule_magic" != "PBR9" ||
      "$encounter_magic" != "PEC1" ||
      "$trainer_magic" != "PTC1" ||
      "$boot_magic" != "PBT2" ||
      "$campaign_magic" != "PCPR" ||
      "$audio_magic" != "PRA3" ]]; then
    rom="$repo_root/../native-gb-pokemon-red/roms/pokemon_red.gb"
    if [[ ! -f "$rom" ]]; then
        echo "Imported runtime data is stale and the canonical ROM is missing: $rom" >&2
        exit 1
    fi
    cmake --build "$repo_root/$build_dir" \
        --target native-gb-pokemon-red-modern-import
    "$repo_root/$build_dir/native-gb-pokemon-red-modern-import" \
        "$rom" "$import_root"
fi

exec "$repo_root/$build_dir/native-gb-pokemon-red-modern" "$@"
