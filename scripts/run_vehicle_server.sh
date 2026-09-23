#!/usr/bin/env bash
set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
scenario=steady
interval=1.0
interface=vcan0
someip_profile=local
gateway_pid=""
ecu_pid=""

usage() { printf 'Usage: %s [--scenario steady|acceleration] [--interval SECONDS] [--interface NAME] [--someip-profile local|pi]\n' "$0"; }
log() { printf '[SETUP] %s\n' "$*"; }
prefix_lines() {
    local component="$1" line
    while IFS= read -r line || [[ -n "$line" ]]; do
        printf '[%s] %s\n' "$component" "$line"
    done
}
stop_child() {
    if [[ -n "$1" ]] && kill -0 "$1" 2>/dev/null; then kill -TERM "$1" 2>/dev/null || true; fi
}
cleanup() {
    local status=$? pid attempt
    trap - EXIT INT TERM
    stop_child "$ecu_pid"
    stop_child "$gateway_pid"
    for pid in "$ecu_pid" "$gateway_pid"; do
        [[ -n "$pid" ]] || continue
        for ((attempt=0; attempt<30; attempt++)); do
            kill -0 "$pid" 2>/dev/null || break
            sleep 0.1
        done
        if kill -0 "$pid" 2>/dev/null; then
            printf '[SETUP] Child %s did not stop; sending SIGKILL\n' "$pid" >&2
            kill -KILL "$pid" 2>/dev/null || true
        fi
        wait "$pid" 2>/dev/null || true
    done
    exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

while (($#)); do
    case "$1" in
        --scenario|--interval|--interface|--someip-profile)
            if (($# < 2)); then usage >&2; exit 2; fi
            case "$1" in
                --scenario) scenario="$2" ;;
                --interval) interval="$2" ;;
                --interface) interface="$2" ;;
                --someip-profile) someip_profile="$2" ;;
            esac
            shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) usage >&2; printf '[SETUP] Unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done
if [[ "$scenario" != steady && "$scenario" != acceleration ]]; then
    printf '[SETUP] Scenario must be steady or acceleration\n' >&2; exit 2
fi
if [[ "$someip_profile" != local && "$someip_profile" != pi ]]; then
    printf '[SETUP] SOME/IP profile must be local or pi\n' >&2; exit 2
fi
if [[ ! "$interface" =~ ^[A-Za-z0-9_.-]{1,15}$ ]]; then
    printf '[SETUP] Invalid CAN interface name: %s\n' "$interface" >&2; exit 2
fi

python_bin="${VEHICLE_PYTHON:-}"
if [[ -z "$python_bin" ]]; then
    if [[ -x "$repo_root/.venv/bin/python" ]]; then
        python_bin="$repo_root/.venv/bin/python"
    else
        python_bin=python3
    fi
fi
if ! command -v "$python_bin" >/dev/null 2>&1; then
    printf '[SETUP] Python executable not found: %s\n' "$python_bin" >&2; exit 1
fi
if ! "$python_bin" -c 'import math,sys; value=float(sys.argv[1]); assert math.isfinite(value) and value > 0' "$interval" >/dev/null 2>&1; then
    printf '[SETUP] --interval must be a finite positive number\n' >&2; exit 2
fi
gateway_bin="${VEHICLE_GATEWAY_BIN:-$repo_root/build/cpp/apps/vehicle_gateway}"
if [[ ! -x "$gateway_bin" ]]; then
    printf '[SETUP] Gateway executable missing: %s\n' "$gateway_bin" >&2
    printf '[SETUP] Build it with: cmake -S . -B build -G Ninja && cmake --build build\n' >&2
    exit 1
fi
someip_config="$repo_root/config/someip/provider.json"
if [[ "$someip_profile" == pi ]]; then someip_config="$repo_root/config/someip/provider_pi.json"; fi
if [[ ! -f "$someip_config" ]]; then
    printf '[SETUP] vSomeIP provider config missing\n' >&2; exit 1
fi
if ! PYTHONPATH="$repo_root/python" "$python_bin" -c 'import can, virtual_ecu.__main__' >/dev/null 2>&1; then
    printf '[SETUP] Virtual ECU cannot start with %s; install python-can in that environment\n' "$python_bin" >&2
    exit 1
fi
if ! command -v ip >/dev/null 2>&1; then
    printf '[SETUP] iproute2 is required (ip command missing)\n' >&2; exit 1
fi
if ! ip link show dev "$interface" >/dev/null 2>&1; then
    printf '[SETUP] %s is missing. Create it explicitly:\n' "$interface" >&2
    printf '[SETUP]   sudo modprobe vcan\n' >&2
    printf '[SETUP]   sudo ip link add dev %s type vcan\n' "$interface" >&2
    printf '[SETUP]   sudo ip link set dev %s up\n' "$interface" >&2
    exit 1
fi
if ! ip -o link show dev "$interface" | grep -Eq '<[^>]*UP'; then
    printf '[SETUP] %s exists but is down. Bring it up explicitly: sudo ip link set dev %s up\n' "$interface" "$interface" >&2
    exit 1
fi
if [[ "$someip_profile" == pi ]]; then
    if ! ip -o -4 addr show dev eth0 | grep -q 'inet 192[.]168[.]50[.]2/24'; then
        printf '[SETUP] Pi profile requires 192.168.50.2/24 on eth0; inspect ip addr show dev eth0\n' >&2
        exit 1
    fi
    if ! ip route get 224.244.224.245 | grep -Eq 'dev eth0( |$)'; then
        printf '[SETUP] SD multicast route is not eth0; inspect ip route get 224.244.224.245\n' >&2
        printf '[SETUP] Add it explicitly if appropriate: sudo ip route replace 224.244.224.245/32 dev eth0\n' >&2
        exit 1
    fi
fi
log "$interface available"
log "Using Python: $python_bin"

export VSOMEIP_CONFIGURATION="$someip_config"
export VSOMEIP_APPLICATION_NAME=vehicle-provider
log "SOME/IP profile: $someip_profile; config: $VSOMEIP_CONFIGURATION; application: $VSOMEIP_APPLICATION_NAME"
if [[ "$someip_profile" == pi ]]; then log "Ethernet: 192.168.50.2/24 on eth0; SD multicast route via eth0"; fi
log "Starting vehicle gateway..."
"$gateway_bin" --interface "$interface" > >(prefix_lines GATEWAY) 2>&1 &
gateway_pid=$!
log "Gateway PID: $gateway_pid"
sleep 0.3
if ! kill -0 "$gateway_pid" 2>/dev/null; then
    printf '[SETUP] Gateway exited during startup\n' >&2; exit 1
fi

log "Starting Virtual ECU ($scenario scenario, ${interval}s interval)..."
PYTHONPATH="$repo_root/python" "$python_bin" -u -m virtual_ecu \
    --interface "$interface" --interval "$interval" --scenario "$scenario" \
    > >(prefix_lines ECU) 2>&1 &
ecu_pid=$!
log "ECU PID: $ecu_pid"
sleep 0.3
if ! kill -0 "$ecu_pid" 2>/dev/null; then
    printf '[SETUP] Virtual ECU exited during startup\n' >&2; exit 1
fi

log "Server running. Press Ctrl+C to stop both processes."
if wait -n "$gateway_pid" "$ecu_pid"; then
    printf '[SETUP] A child exited unexpectedly\n' >&2
    exit 1
else
    status=$?
    printf '[SETUP] A child exited with status %s\n' "$status" >&2
    exit "$status"
fi
