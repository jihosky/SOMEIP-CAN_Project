#!/usr/bin/env bash
set -Eeuo pipefail
script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
python_bin="${VEHICLE_PYTHON:-$repo_root/.venv/bin/python}"
if [[ ! -x "$python_bin" ]]; then python_bin=python3; fi
PYTHONPATH="$repo_root/python${PYTHONPATH:+:$PYTHONPATH}" exec "$python_bin" -m vehicle_dashboard "$@"
