#!/usr/bin/env bash
set -Eeuo pipefail
script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
usage() { printf 'Usage: %s [--someip-profile local|pc|work] method | subscribe [COUNT] | body | body-subscribe [COUNT] | door INDEX open|close\n' "$0"; }
someip_profile=local
if [[ "${1:-}" == --someip-profile ]]; then
    if (($# < 2)); then usage >&2; exit 2; fi
    someip_profile="$2"
    shift 2
fi
case "$someip_profile" in
    local) someip_config="$repo_root/config/someip/client.json" ;;
    pc) someip_config="$repo_root/config/someip/client_pc.json" ;;
    work) someip_config="$repo_root/config/someip/client_office.json" ;;
    *) printf '[CLIENT] SOME/IP profile must be local, pc, or work\n' >&2; exit 2 ;;
esac
if (($# < 1)); then usage >&2; exit 2; fi
mode="$1"
shift
case "$mode" in
    method|body)
        if (($# != 0)); then usage >&2; exit 2; fi ;;
    subscribe|body-subscribe)
        if (($# > 1)); then usage >&2; exit 2; fi
        count="${1:-3}"
        if [[ ! "$count" =~ ^[0-9]+$ ]] || ((10#$count < 1 || 10#$count > 10000)); then
            printf '[CLIENT] Event count must be an integer from 1 to 10000\n' >&2
            exit 2
        fi ;;
    door)
        if (($# != 2)) || [[ ! "$1" =~ ^[0-4]$ ]] || [[ "$2" != open && "$2" != close ]]; then
            usage >&2; exit 2
        fi
        door_index="$1"
        door_action="$2" ;;
    --help|-h) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
esac

if [[ "$mode" == body || "$mode" == body-subscribe || "$mode" == door ]]; then
    client_bin="${VEHICLE_BODY_CLIENT_BIN:-$repo_root/build/cpp/someip/client/vehicle_body_client}"
else
    client_bin="${VEHICLE_CLIENT_BIN:-$repo_root/build/cpp/someip/client/vehicle_data_client}"
fi
if [[ ! -x "$client_bin" ]]; then
    printf '[CLIENT] Client executable missing: %s\n' "$client_bin" >&2
    printf '[CLIENT] Build it with: cmake -S . -B build -G Ninja && cmake --build build\n' >&2
    exit 1
fi
if [[ ! -f "$someip_config" ]]; then
    printf '[CLIENT] vSomeIP client config missing: %s\n' "$someip_config" >&2; exit 1
fi
if [[ "$someip_profile" != local ]]; then
    python3 "$script_dir/check_someip_network.py" "$someip_config" || exit 1
fi
prefix_lines() {
    local line
    while IFS= read -r line || [[ -n "$line" ]]; do
        printf '[CLIENT] %s\n' "$line"
    done
}
export VSOMEIP_CONFIGURATION="$someip_config"
export VSOMEIP_APPLICATION_NAME=vehicle-client
printf '[CLIENT] SOME/IP profile: %s; config: %s; application: %s\n' \
    "$someip_profile" "$VSOMEIP_CONFIGURATION" "$VSOMEIP_APPLICATION_NAME"
case "$mode" in
    method) arguments=(--timeout 5) ;;
    subscribe) arguments=(--timeout 15 --subscribe "$count") ;;
    body) arguments=(read) ;;
    body-subscribe) arguments=(subscribe "$count") ;;
    door) arguments=(door "$door_index" "$door_action") ;;
esac
if "$client_bin" "${arguments[@]}" 2>&1 | prefix_lines; then exit 0; fi
printf '[CLIENT] Service unavailable, timed out, or request failed. Check the Pi server and its logs.\n' >&2
exit 1
