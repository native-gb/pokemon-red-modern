#!/usr/bin/env bash
set -euo pipefail

socket_path="${POKERED_MODERN_INPUT_SOCKET:-/tmp/pokered-modern-input.sock}"
if [[ ! -S "$socket_path" ]]; then
    echo "Developer input socket is not available: $socket_path" >&2
    exit 1
fi
if [[ $# -eq 0 ]]; then
    echo "Usage: $0 COMMAND..." >&2
    exit 2
fi

printf '%s\n' "$*" | nc -u -U -q 0 "$socket_path"
