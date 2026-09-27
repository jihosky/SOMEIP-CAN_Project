#!/usr/bin/env bash
set -Eeuo pipefail
script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
usage() { printf 'Usage: %s [--someip-profile default|pc] method | subscribe [EVENT_COUNT] | body | body-subscribe [EVENT_COUNT] | door INDEX open|close\n' "$0"; }

someip_profile=default
if [[ "${1:-}" == --someip-profile ]]; then
    if (($# < 2)); then usage >&2; exit 2; fi
    someip_profile="$2"
    shift 2
fi
case "$someip_profile" in
    default) someip_config="$repo_root/config/someip/client.json" ;;
    pc) someip_config="$repo_root/config/someip/client_pc.json" ;;
    *) printf '[CLIENT] Unknown SOME/IP profile: %s\n' "$someip_profile" >&2; exit 2 ;;
esac

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
    body)
        if (($# != 0)); then usage >&2; exit 2; fi ;;
    body-subscribe)
        if (($# > 1)); then usage >&2; exit 2; fi
        count="${1:-3}"
        if [[ ! "$count" =~ ^[0-9]+$ ]] || ((10#$count < 1 || 10#$count > 10000)); then
            printf '[CLIENT] Event count must be 1-10000\n' >&2; exit 2
        fi ;;
    door)
        if (($# != 2)) || [[ ! "$1" =~ ^[0-4]$ ]] ||
            [[ "$2" != open && "$2" != close ]]; then
            usage >&2; exit 2
        fi
        door_index="$1"
        door_action="$2" ;;
    --help|-h) usage; exit 0 ;;
    *) usage >&2; printf '[CLIENT] Unknown mode: %s\n' "$mode" >&2; exit 2 ;;
esac

client_bin="${VEHICLE_CLIENT_BIN:-$repo_root/build/cpp/someip/client/vehicle_data_client}"
if [[ "$mode" == body || "$mode" == body-subscribe || "$mode" == door ]]; then
    client_bin="${VEHICLE_BODY_CLIENT_BIN:-$repo_root/build/cpp/someip/client/vehicle_body_client}"
fi
if [[ ! -x "$client_bin" ]]; then
    printf '[CLIENT] Client executable missing: %s\n' "$client_bin" >&2
    printf '[CLIENT] Build it with: cmake -S . -B build -G Ninja && cmake --build build\n' >&2
    exit 1
fi
if [[ ! -f "$someip_config" ]]; then
    printf '[CLIENT] vSomeIP client config missing\n' >&2; exit 1
fi
prefix_lines() {
    local line
    while IFS= read -r line || [[ -n "$line" ]]; do
        printf '[CLIENT] %s\n' "$line"
    done
}
export VSOMEIP_CONFIGURATION="$someip_config"
export VSOMEIP_APPLICATION_NAME=vehicle-client
printf '[CLIENT] VSOMEIP_CONFIGURATION=%s\n' "$VSOMEIP_CONFIGURATION"
printf '[CLIENT] VSOMEIP_APPLICATION_NAME=%s\n' "$VSOMEIP_APPLICATION_NAME"
if [[ "$mode" == method ]]; then
    if "$client_bin" --timeout "$timeout" 2>&1 | prefix_lines; then exit 0; fi
elif [[ "$mode" == subscribe ]]; then
    if "$client_bin" --timeout "$timeout" --subscribe "$count" 2>&1 | prefix_lines; then exit 0; fi
elif [[ "$mode" == body ]]; then
    if "$client_bin" read 2>&1 | prefix_lines; then exit 0; fi
elif [[ "$mode" == body-subscribe ]]; then
    if "$client_bin" subscribe "$count" 2>&1 | prefix_lines; then exit 0; fi
else
    if "$client_bin" door "$door_index" "$door_action" 2>&1 | prefix_lines; then exit 0; fi
fi
printf '[CLIENT] Service unavailable, timed out, or request failed. Start ./scripts/run_vehicle_server.sh and check its logs.\n' >&2
exit 1
