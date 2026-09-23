#!/usr/bin/env bash
set -Eeuo pipefail
script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
server="$script_dir/run_vehicle_server.sh"
client="$script_dir/run_vehicle_client.sh"
server_pid=""
if (($# != 0)); then printf 'Usage: %s\n' "$0" >&2; exit 2; fi

cleanup() {
    local status=$? attempt
    trap - EXIT INT TERM
    if [[ -n "$server_pid" ]] && kill -0 "$server_pid" 2>/dev/null; then
        kill -TERM "$server_pid" 2>/dev/null || true
        for ((attempt=0; attempt<50; attempt++)); do
            kill -0 "$server_pid" 2>/dev/null || break
            sleep 0.1
        done
        if kill -0 "$server_pid" 2>/dev/null; then kill -KILL "$server_pid" 2>/dev/null || true; fi
    fi
    [[ -z "$server_pid" ]] || wait "$server_pid" 2>/dev/null || true
    exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

printf '[DEMO] Starting steady server stack\n'
"$server" --scenario steady --interval 0.2 &
server_pid=$!
sleep 1
for attempt in 1 2 3; do
    if ! kill -0 "$server_pid" 2>/dev/null; then
        printf '[DEMO] Server exited before verification\n' >&2; exit 1
    fi
    if result="$("$client" method 2>&1)"; then
        printf '%s\n' "$result"
        if [[ "$result" == *"Vehicle speed: 123.45 km/h"* &&
              "$result" == *"Engine RPM: 2500 rpm"* &&
              "$result" == *"Coolant temperature: 85 C"* ]]; then
            printf '[DEMO] PASS: Virtual ECU -> vcan -> gateway -> VehicleService -> SOME/IP client\n'
            exit 0
        fi
        printf '[DEMO] Client returned unexpected vehicle values\n' >&2; exit 1
    fi
    printf '[DEMO] Waiting for SOME/IP service (attempt %s/3)\n' "$attempt"
    sleep 1
done
printf '[DEMO] Client verification failed: %s\n' "$result" >&2
exit 1
