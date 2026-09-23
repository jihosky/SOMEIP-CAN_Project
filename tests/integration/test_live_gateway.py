"""Milestone 2B virtual ECU to SOME/IP client integration test."""

import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import time

import pytest


def stop_process(process):
    if process.poll() is None:
        process.send_signal(signal.SIGINT)
    try:
        process.communicate(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate()
        pytest.fail(f"process {process.args} hung during shutdown")


@pytest.mark.integration
def test_virtual_ecu_to_someip_client():
    root = Path(__file__).resolve().parents[2]
    gateway = root / "build" / "cpp" / "apps" / "vehicle_gateway"
    client = root / "build" / "cpp" / "someip" / "client" / "vehicle_data_client"
    if not gateway.is_file() or not client.is_file():
        pytest.skip("Build the integrated gateway and SOME/IP client first")
    interface = os.environ.get("CAN_INTERFACE", "vcan0")
    if not (Path("/sys/class/net") / interface).exists():
        pytest.skip(f"CAN interface {interface!r} is unavailable")

    gateway_env = os.environ.copy()
    gateway_env["VSOMEIP_CONFIGURATION"] = str(root / "config/someip/provider.json")
    client_env = os.environ.copy()
    client_env["VSOMEIP_CONFIGURATION"] = str(root / "config/someip/client.json")
    sender_env = os.environ.copy()
    sender_env["PYTHONPATH"] = str(root / "python")

    gateway_process = subprocess.Popen(
        [str(gateway), "--interface", interface],
        env=gateway_env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    sender_process = None
    try:
        sender_process = subprocess.Popen(
            [sys.executable, "-m", "virtual_ecu", "--interface", interface,
             "--interval", "0.1"],
            env=sender_env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )
        output = bytearray()
        deadline = time.monotonic() + 6
        while b"vehicle_speed_kph=123.45 engine_rpm=2500 coolant_temperature_c=85" not in output:
            if gateway_process.poll() is not None:
                pytest.fail(f"gateway exited: {gateway_process.stderr.read().decode()}")
            if sender_process.poll() is not None:
                pytest.fail(f"sender exited: {sender_process.stderr.read().decode()}")
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                pytest.fail(f"no live vehicle data within 6 seconds: {output.decode()}")
            ready, _, _ = select.select([gateway_process.stdout], [], [], remaining)
            if ready:
                output.extend(os.read(gateway_process.stdout.fileno(), 4096))

        result = subprocess.run(
            [str(client), "--timeout", "5"],
            env=client_env,
            capture_output=True,
            text=True,
            timeout=10,
        )
        assert result.returncode == 0, result.stdout + result.stderr
        assert "Vehicle speed: 123.45 km/h" in result.stdout
        assert "Engine RPM: 2500 rpm" in result.stdout
        assert "Coolant temperature: 85 C" in result.stdout
    finally:
        if sender_process is not None:
            stop_process(sender_process)
        stop_process(gateway_process)
    assert gateway_process.returncode == 0
