"""Local VehicleService -> vSomeIP provider -> client test."""

import os
from pathlib import Path
import select
import subprocess
import time

import pytest


@pytest.mark.integration
def test_vehicle_service_provider_client():
    root = Path(__file__).resolve().parents[2]
    provider = root / "build" / "cpp" / "someip" / "provider" / "vehicle_data_provider"
    client = root / "build" / "cpp" / "someip" / "client" / "vehicle_data_client"
    if not provider.is_file() or not client.is_file():
        pytest.skip("Build the C++ provider and client first")

    provider_env = os.environ.copy()
    provider_env["VSOMEIP_CONFIGURATION"] = str(root / "config/someip/provider.json")
    client_env = os.environ.copy()
    client_env["VSOMEIP_CONFIGURATION"] = str(root / "config/someip/client.json")
    process = subprocess.Popen(
        [str(provider), "--demo"],
        env=provider_env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=False,
    )
    try:
        output = bytearray()
        deadline = time.monotonic() + 5
        while b"VehicleDataService provider ready" not in output:
            if process.poll() is not None:
                pytest.fail(f"provider exited: {process.stderr.read().decode()}")
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                pytest.fail(f"provider did not start: {output.decode()}")
            ready, _, _ = select.select([process.stdout], [], [], remaining)
            if ready:
                output.extend(os.read(process.stdout.fileno(), 4096))

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
        process.terminate()
        try:
            process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate()
