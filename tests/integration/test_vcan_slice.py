"""Run explicitly with pytest tests/integration -m integration."""

import os
from pathlib import Path
import re
import select
import subprocess
import sys
import time

import pytest


@pytest.mark.integration
def test_virtual_ecu_reaches_socketcan_receiver():
    root = Path(__file__).resolve().parents[2]
    receiver = root / "build" / "cpp" / "can_gateway" / "socketcan_receiver"
    if not receiver.is_file():
        pytest.skip("Build the C++ receiver first: cmake -S . -B build && cmake --build build")

    interface = os.environ.get("CAN_INTERFACE", "vcan0")
    if not (Path("/sys/class/net") / interface).exists():
        pytest.skip(f"CAN interface {interface!r} is unavailable")

    command = [sys.executable, "-m", "virtual_ecu", "--interface", interface, "--interval", "0.1"]
    environment = os.environ.copy()
    environment["PYTHONPATH"] = str(root / "python")

    receiver_process = subprocess.Popen(
        [str(receiver), "--interface", interface],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=False,
    )
    sender_process = None
    output = bytearray()
    try:
        sender_process = subprocess.Popen(
            command,
            env=environment,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
        )
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if receiver_process.poll() is not None:
                pytest.fail(f"receiver exited: {receiver_process.stderr.read().decode()}")
            if sender_process.poll() is not None:
                pytest.fail(f"sender exited: {sender_process.stderr.read()}")
            ready, _, _ = select.select([receiver_process.stdout], [], [], 0.2)
            if ready:
                output.extend(os.read(receiver_process.stdout.fileno(), 4096))
                if b"coolant_temperature_c=85" in output:
                    break
        else:
            pytest.fail(f"no expected frame within 5 seconds; output: {output!r}")
    finally:
        if sender_process is not None:
            sender_process.terminate()
            try:
                sender_process.communicate(timeout=3)
            except subprocess.TimeoutExpired:
                sender_process.kill()
                sender_process.communicate()
        receiver_process.terminate()
        try:
            receiver_process.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            receiver_process.kill()
            receiver_process.communicate()

    assert re.search(
        r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z "
        r"ID=0x100 DLC=8 DATA=39 30 C4 09 7D 00 00 00\n"
        r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z "
        r"vehicle_speed_kph=123\.45 engine_rpm=2500 coolant_temperature_c=85",
        output.decode(),
    ), f"receiver output: {output!r}"
