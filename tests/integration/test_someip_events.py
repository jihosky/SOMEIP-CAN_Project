"""Dynamic virtual ECU to vSomeIP VehicleData event integration test."""

import os
from pathlib import Path
import select
import signal
import socket
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


def wait_for_output(process, expected, timeout):
    output = bytearray()
    deadline = time.monotonic() + timeout
    while expected not in output:
        if process.poll() is not None:
            stderr = process.stderr.read().decode()
            pytest.fail(f"process exited before {expected!r}: {output!r} {stderr}")
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            pytest.fail(f"timed out waiting for {expected!r}: {output!r}")
        ready, _, _ = select.select([process.stdout], [], [], remaining)
        if ready:
            output.extend(os.read(process.stdout.fileno(), 4096))
    return output


@pytest.mark.integration
def test_dynamic_vehicle_data_events():
    root = Path(__file__).resolve().parents[2]
    gateway = root / "build" / "cpp" / "apps" / "vehicle_gateway"
    client = root / "build" / "cpp" / "someip" / "client" / "vehicle_data_client"
    if not gateway.is_file() or not client.is_file():
        pytest.skip("Build the gateway and client first")
    interface = os.environ.get("CAN_INTERFACE", "vcan0")
    try:
        socket.if_nametoindex(interface)
    except OSError:
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
    client_process = None
    sender_process = None
    try:
        wait_for_output(gateway_process, b"Integrated vehicle gateway ready", 5)
        client_process = subprocess.Popen(
            [str(client), "--timeout", "8", "--subscribe", "3"],
            env=client_env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        prefix = wait_for_output(client_process, b"Subscribed to VehicleData events", 5)
        sender_process = subprocess.Popen(
            [
                sys.executable,
                "-m",
                "virtual_ecu",
                "--interface",
                interface,
                "--interval",
                "0.15",
                "--scenario",
                "acceleration",
            ],
            env=sender_env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )
        stdout, stderr = client_process.communicate(timeout=10)
        output = (prefix + stdout).decode()
        assert client_process.returncode == 0, output + stderr.decode()
        expected = [
            "VehicleData event: speed=20.00 km/h rpm=1200 coolant=70 C",
            "VehicleData event: speed=40.00 km/h rpm=1800 coolant=71 C",
            "VehicleData event: speed=60.00 km/h rpm=2400 coolant=72 C",
        ]
        positions = [output.find(line) for line in expected]
        assert all(position >= 0 for position in positions), output
        assert positions == sorted(positions), output
    finally:
        if sender_process is not None:
            stop_process(sender_process)
        if client_process is not None and client_process.poll() is None:
            stop_process(client_process)
        stop_process(gateway_process)
    assert gateway_process.returncode == 0
