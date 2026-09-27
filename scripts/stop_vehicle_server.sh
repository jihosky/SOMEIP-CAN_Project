#!/usr/bin/env bash
set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
gateway_bin="$repo_root/build/cpp/apps/vehicle_gateway"
found=0

while IFS= read -r gateway_pid; do
    [[ -n "$gateway_pid" ]] || continue
    [[ -e "/proc/$gateway_pid/exe" ]] || continue
    gateway_executable="$(readlink "/proc/$gateway_pid/exe" || true)"
    gateway_executable="${gateway_executable% (deleted)}"
    if [[ "$gateway_executable" != "$gateway_bin" ]]; then
        continue
    fi
    found=1
    launcher_pid="$(ps -o ppid= -p "$gateway_pid" | tr -d ' ')"
    launcher_command=""
    if [[ -n "$launcher_pid" && -r "/proc/$launcher_pid/cmdline" ]]; then
        launcher_command="$(tr '\0' ' ' < "/proc/$launcher_pid/cmdline")"
    fi
    if [[ "$launcher_command" == *run_vehicle_server.sh* ]]; then
        printf '[SETUP] Stopping launcher PID %s (gateway PID %s)\n' \
            "$launcher_pid" "$gateway_pid"
        kill -TERM "$launcher_pid" 2>/dev/null || true
    else
        printf '[SETUP] Stopping orphan gateway PID %s\n' "$gateway_pid"
        kill -TERM "$gateway_pid" 2>/dev/null || true
    fi
    for ((attempt=0; attempt<40; attempt++)); do
        kill -0 "$gateway_pid" 2>/dev/null || break
        sleep 0.1
    done
    if kill -0 "$gateway_pid" 2>/dev/null; then
        printf '[SETUP] Gateway PID %s did not stop; inspect it manually.\n' \
            "$gateway_pid" >&2
        exit 1
    fi
done < <(pgrep -x vehicle_gateway || true)

if ((found == 0)); then
    printf '[SETUP] No project vehicle_gateway is running.\n'
else
    printf '[SETUP] Server stopped.\n'
fi
