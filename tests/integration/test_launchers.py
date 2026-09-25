"""Launcher integration checks on an existing vCAN interface."""

import errno
import os
from pathlib import Path
import re
import select
import signal
import subprocess
import time

import pytest

ROOT = Path(__file__).resolve().parents[2]
SERVER = ROOT / "scripts/run_vehicle_server.sh"
CLIENT = ROOT / "scripts/run_vehicle_client.sh"
DEMO = ROOT / "scripts/run_demo.sh"


def wait_for_text(process, marker, timeout=8):
    output = bytearray()
    deadline = time.monotonic() + timeout
    while marker not in output:
        if process.poll() is not None:
            output.extend(process.stdout.read())
            pytest.fail(f"exited before {marker!r}: {output.decode(errors='replace')}")
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            pytest.fail(f"timed out waiting for {marker!r}: {output.decode(errors='replace')}")
        ready, _, _ = select.select([process.stdout], [], [], remaining)
        if ready:
            output.extend(os.read(process.stdout.fileno(), 4096))
    return bytes(output)


def process_exists(pid):
    try:
        os.kill(pid, 0)
    except OSError as error:
        if error.errno == errno.ESRCH:
            return False
        raise
    return True


@pytest.mark.integration
def test_argument_and_missing_binary_errors():
    cases = [
        ([str(SERVER), "--scenario", "wrong"], {}, 2, "Scenario must be"),
        ([str(SERVER), "--someip-profile", "wrong"], {}, 2, "SOME/IP profile must be"),
        ([str(CLIENT), "--someip-profile", "wrong", "method"], {}, 2, "SOME/IP profile must be"),
        ([str(SERVER), "--interval", "0"], {}, 2, "--interval must be"),
        ([str(SERVER), "--interface", "vcan999"], {}, 1, "is missing"),
        ([str(SERVER)], {"VEHICLE_GATEWAY_BIN": "/tmp/missing-vehicle-gateway"}, 1, "Gateway executable missing"),
        ([str(CLIENT), "subscribe", "zero"], {}, 2, "Event count must be"),
        ([str(CLIENT), "body-subscribe", "zero"], {}, 2, "Event count must be"),
        ([str(CLIENT), "door", "5", "open"], {}, 2, "Usage:"),
        ([str(CLIENT), "door", "0", "lock"], {}, 2, "Usage:"),
        ([str(CLIENT), "method"], {"VEHICLE_CLIENT_BIN": "/tmp/missing-vehicle-client"}, 1, "Client executable missing"),
        ([str(CLIENT), "body"], {"VEHICLE_BODY_CLIENT_BIN": "/tmp/missing-vehicle-body-client"}, 1, "Client executable missing"),
    ]
    for command, extra_env, code, message in cases:
        result = subprocess.run(
            command, env={**os.environ, **extra_env}, capture_output=True, text=True
        )
        assert result.returncode == code, result.stdout + result.stderr
        assert message in result.stderr


@pytest.mark.integration
def test_two_terminal_event_flow_and_shutdown():
    interface = os.environ.get("CAN_INTERFACE", "vcan0")
    if not (Path("/sys/class/net") / interface).exists():
        pytest.skip(f"{interface} unavailable")
    server = subprocess.Popen(
        [str(SERVER), "--scenario", "acceleration", "--interval", "0.2",
         "--interface", interface],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    child_pids = []
    try:
        prefix = wait_for_text(server, b"Server running")
        assert b"config/someip/provider.json; application: vehicle-provider" in prefix
        child_pids = [int(value) for value in re.findall(rb"(?:Gateway|ECU) PID: (\d+)", prefix)]
        assert len(child_pids) == 2, prefix.decode()
        result = subprocess.run(
            [str(CLIENT), "subscribe", "3"], capture_output=True, text=True, timeout=20
        )
        assert result.returncode == 0, result.stdout + result.stderr
        assert "config/someip/client.json; application: vehicle-client" in result.stdout
        for value in ("speed=20.00", "speed=40.00", "speed=60.00"):
            assert value in result.stdout, result.stdout
    finally:
        if server.poll() is None:
            server.send_signal(signal.SIGINT)
        try:
            server.communicate(timeout=6)
        except subprocess.TimeoutExpired:
            server.kill()
            server.communicate()
            pytest.fail("server launcher hung on SIGINT")
    assert server.returncode == 130
    assert all(not process_exists(pid) for pid in child_pids)


@pytest.mark.integration
def test_quick_demo():
    interface = os.environ.get("CAN_INTERFACE", "vcan0")
    if not (Path("/sys/class/net") / interface).exists():
        pytest.skip(f"{interface} unavailable")
    result = subprocess.run([str(DEMO)], capture_output=True, text=True, timeout=25)
    assert result.returncode == 0, result.stdout + result.stderr
    for value in (
        "[DEMO] PASS:",
        "Vehicle speed: 123.45 km/h",
        "Engine RPM: 2500 rpm",
        "Coolant temperature: 85 C",
    ):
        assert value in result.stdout
