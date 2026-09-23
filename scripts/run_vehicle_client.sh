#!/usr/bin/env bash
set -Eeuo pipefail
script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
usage() { printf 'Usage: %s method | subscribe [EVENT_COUNT]\n' "$0"; }

if (($# < 1)); then usage >&2; exit 2; fi
mode="$1"
shift
case "$mode" in
    method)
        if (($# != 0)); then usage >&2; exit 2; fi
        timeout=5 ;;
    subscribe)
        if (($# > 1)); then usage >&2; exit 2; fi
        count="${1:-3}"
        if [[ ! "$count" =~ ^[0-9]+$ ]] || ((10#$count < 1 || 10#$count > 10000)); then
            printf '[CLIENT] Event count must be an integer from 1 to 10000\n' >&2
            exit 2
        fi
        timeout=15 ;;
    --help|-h) usage; exit 0 ;;
    *) usage >&2; printf '[CLIENT] Unknown mode: %s\n' "$mode" >&2; exit 2 ;;
esac

client_bin="${VEHICLE_CLIENT_BIN:-$repo_root/build/cpp/someip/client/vehicle_data_client}"
if [[ ! -x "$client_bin" ]]; then
    printf '[CLIENT] Client executable missing: %s\n' "$client_bin" >&2
    printf '[CLIENT] Build it with: cmake -S . -B build -G Ninja && cmake --build build\n' >&2
    exit 1
fi
if [[ ! -f "$repo_root/config/someip/client.json" ]]; then
    printf '[CLIENT] vSomeIP client config missing\n' >&2; exit 1
fi
prefix_lines() {
    local line
    while IFS= read -r line || [[ -n "$line" ]]; do
        printf '[CLIENT] %s\n' "$line"
    done
}
export VSOMEIP_CONFIGURATION="$repo_root/config/someip/client.json"
if [[ "$mode" == method ]]; then
    if "$client_bin" --timeout "$timeout" 2>&1 | prefix_lines; then exit 0; fi
else
    if "$client_bin" --timeout "$timeout" --subscribe "$count" 2>&1 | prefix_lines; then exit 0; fi
fi
printf '[CLIENT] Service unavailable, timed out, or request failed. Start ./scripts/run_vehicle_server.sh and check its logs.\n' >&2
exit 1
