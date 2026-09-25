"""Prove SOME/IP door command -> vCAN ECU -> status event on one Linux host."""

import json
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile
import time

import pytest


ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.integration
def test_body_command_round_trip():
    if not Path("/sys/class/net/vcan0").exists():
        pytest.skip("vcan0 is unavailable")
    route = subprocess.run(
        ["ip", "-4", "route", "get", "224.244.224.245"],
        capture_output=True, text=True,
    )
    match = re.search(r"\bsrc (\d+\.\d+\.\d+\.\d+)", route.stdout)
    if route.returncode or not match:
        pytest.skip("No IPv4 multicast source route")
    unicast = match.group(1)
    gateway = ROOT / "build/cpp/apps/vehicle_gateway"
    client = ROOT / "build/cpp/someip/client/vehicle_body_client"
    if not gateway.exists() or not client.exists():
        pytest.skip("Build the C++ binaries first")
    with tempfile.TemporaryDirectory() as tmp:
        provider_config = json.loads((ROOT / "config/someip/provider_pi.json").read_text())
        provider_config["unicast"] = unicast
        client_config = json.loads((ROOT / "config/someip/client.json").read_text())
        client_config["unicast"] = unicast
        client_config["routing"] = "vehicle-provider"
        provider_path = Path(tmp) / "provider.json"
        client_path = Path(tmp) / "client.json"
        provider_path.write_text(json.dumps(provider_config))
        client_path.write_text(json.dumps(client_config))

        gateway_env = {**os.environ, "VSOMEIP_CONFIGURATION": str(provider_path),
                       "VSOMEIP_APPLICATION_NAME": "vehicle-provider"}
        client_env = {**os.environ, "VSOMEIP_CONFIGURATION": str(client_path),
                      "VSOMEIP_APPLICATION_NAME": "vehicle-client"}
        ecu_env = {**os.environ, "PYTHONPATH": str(ROOT / "python")}
        gateway_process = subprocess.Popen(
            [str(gateway), "--interface", "vcan0"], env=gateway_env,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        ecu_process = subprocess.Popen(
            [str(ROOT / ".venv/bin/python"), "-m", "virtual_ecu", "--interface",
             "vcan0", "--interval", "0.2"], env=ecu_env,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        try:
            initial = None
            for _ in range(20):
                initial = subprocess.run(
                    [str(client), "read"], env=client_env,
                    capture_output=True, text=True, timeout=14,
                )
                if initial.returncode == 0:
                    break
                time.sleep(0.1)
            assert initial is not None and initial.returncode == 0, initial.stdout + initial.stderr
            assert "BodyStatus flags=0x0" in initial.stdout
            for command, expected in [
                (["door", "0", "open"], "driver_door=open"),
                (["read"], "BodyStatus flags=0x1"),
                (["door", "0", "close"], "driver_door=closed"),
                (["read"], "BodyStatus flags=0x0"),
            ]:
                result = subprocess.run(
                    [str(client), *command], env=client_env,
                    capture_output=True, text=True, timeout=14,
                )
                assert result.returncode == 0, result.stdout + result.stderr
                assert expected in result.stdout, result.stdout
        finally:
            for process in (ecu_process, gateway_process):
                if process.poll() is None:
                    process.send_signal(signal.SIGINT)
                try:
                    process.communicate(timeout=4)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.communicate()
