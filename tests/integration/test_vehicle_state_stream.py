"""Production provider/codec/client streaming over local vSomeIP, without CAN."""
import os
from pathlib import Path
import signal
import subprocess
import time

import pytest

ROOT = Path(__file__).resolve().parents[2]
CLIENT = ROOT / "scripts/run_vehicle_client.sh"
PROVIDER = ROOT / "build/cpp/someip/provider/event_provider_fixture"


def wait_text(path, text, process, timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        content = path.read_text(errors="replace")
        if text in content:
            return content
        assert process.poll() is None, content
        time.sleep(.05)
    pytest.fail(path.read_text(errors="replace"))


@pytest.mark.integration
@pytest.mark.parametrize("continuous", [False, True])
def test_samples_and_method_regression(tmp_path, continuous):
    if not PROVIDER.exists():
        pytest.skip("Build event_provider_fixture first")
    provider_log = tmp_path / "provider.log"
    client_log = tmp_path / "client.log"
    env = {**os.environ, "VSOMEIP_CONFIGURATION": str(ROOT / "config/someip/provider.json")}
    with provider_log.open("w") as po, client_log.open("w") as co:
        provider = subprocess.Popen([str(PROVIDER)], env=env, stdin=subprocess.PIPE, stdout=po, stderr=po, text=True)
        client = None
        try:
            wait_text(provider_log, "offer_service requested", provider)
            args = [str(CLIENT), "subscribe"] + ([] if continuous else ["3"])
            client = subprocess.Popen(args, stdout=co, stderr=co, start_new_session=True)
            wait_text(client_log, "VehicleState subscription ACK", client)
            for speed, rpm, temp in [(20,1200,70), (40,1800,71), (60,2400,72)]:
                provider.stdin.write(f"{speed} {rpm} {temp}\n"); provider.stdin.flush()
                wait_text(client_log, f"speed={speed:.2f} km/h rpm={rpm} coolant={temp} C", client)
            if continuous:
                assert client.poll() is None
                provider.stdin.write("60 2400 72\n"); provider.stdin.flush()
                deadline = time.monotonic()+4
                while client_log.read_text().count("VehicleData event:") < 4 and time.monotonic()<deadline:
                    time.sleep(.05)
                assert client_log.read_text().count("VehicleData event:") == 4
                client.send_signal(signal.SIGINT)
            assert client.wait(timeout=5) == 0
            method = subprocess.run([str(CLIENT), "method"], capture_output=True, text=True, timeout=10)
            assert method.returncode == 0, method.stdout+method.stderr
            for expected in ["Vehicle speed: 60.00", "Engine RPM: 2400", "Coolant temperature: 72"]:
                assert expected in method.stdout
        finally:
            if client is not None and client.poll() is None:
                os.killpg(client.pid, signal.SIGTERM)
                client.wait(timeout=5)
            if provider.poll() is None:
                provider.stdin.write("quit\n"); provider.stdin.flush()
                provider.communicate(timeout=5)


@pytest.mark.integration
def test_client_before_provider_and_provider_exit(tmp_path):
    if not PROVIDER.exists():
        pytest.skip("Build event_provider_fixture first")
    clog, plog = tmp_path / "waiting.log", tmp_path / "provider.log"
    env = {**os.environ, "VSOMEIP_CONFIGURATION": str(ROOT / "config/someip/provider.json")}
    with clog.open("w") as co, plog.open("w") as po:
        client = subprocess.Popen([str(CLIENT), "subscribe"], stdout=co, stderr=co)
        provider = None
        try:
            time.sleep(.5)
            assert client.poll() is None
            provider = subprocess.Popen([str(PROVIDER)], env=env, stdin=subprocess.PIPE, stdout=po, stderr=po, text=True)
            wait_text(clog, "VehicleState subscription ACK", client)
            provider.stdin.write("20 1200 70\n"); provider.stdin.flush()
            wait_text(clog, "speed=20.00 km/h rpm=1200 coolant=70 C", client)
            provider.stdin.write("quit\n"); provider.stdin.flush()
            provider.communicate(timeout=5)
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                after_sample = clog.read_text().split("speed=20.00 km/h rpm=1200 coolant=70 C", 1)[1]
                if "VehicleState service unavailable" in after_sample:
                    break
                time.sleep(.05)
            assert "VehicleState service unavailable" in after_sample
            assert client.poll() is None
            client.send_signal(signal.SIGINT)
            assert client.wait(timeout=5) == 0
        finally:
            if client.poll() is None:
                client.terminate(); client.wait(timeout=5)
            if provider is not None and provider.poll() is None:
                provider.terminate(); provider.communicate(timeout=5)


@pytest.mark.integration
def test_malformed_notification_fails_cleanly(tmp_path):
    if not PROVIDER.exists():
        pytest.skip("Build event_provider_fixture first")
    plog, clog = tmp_path / "provider.log", tmp_path / "client.log"
    env = {**os.environ, "VSOMEIP_CONFIGURATION": str(ROOT / "config/someip/provider.json")}
    with plog.open("w") as po, clog.open("w") as co:
        provider = subprocess.Popen([str(PROVIDER)], env=env, stdin=subprocess.PIPE, stdout=po, stderr=po, text=True)
        client = None
        try:
            wait_text(plog, "offer_service requested", provider)
            client = subprocess.Popen([str(CLIENT), "subscribe"], stdout=co, stderr=co)
            wait_text(clog, "VehicleState subscription ACK", client)
            provider.stdin.write("malformed\n"); provider.stdin.flush()
            assert client.wait(timeout=5) == 1
            assert "Malformed VehicleData event payload" in clog.read_text()
        finally:
            if client is not None and client.poll() is None:
                client.terminate(); client.wait(timeout=5)
            if provider.poll() is None:
                provider.stdin.write("quit\n"); provider.stdin.flush()
                provider.communicate(timeout=5)
