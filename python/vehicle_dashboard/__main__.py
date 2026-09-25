"""Local browser dashboard for high-level SOME/IP vehicle and body data."""

from __future__ import annotations

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import subprocess
import threading
import time
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[2]
PAGE = Path(__file__).with_name("dashboard.html")
DATA_PATTERN = re.compile(r"DASHBOARD DATA speed=(\d+(?:\.\d+)?) rpm=(\d+) coolant=(-?\d+)")
BODY_PATTERN = re.compile(r"DASHBOARD (BODY_EVENT|BODY_RESPONSE) flags=(\d+)")
ACK_PATTERN = re.compile(r"DASHBOARD ACK door=0 action=(open|close)")
LEGACY_EVENT_PATTERN = re.compile(
    r"VehicleData event: speed=(\d+(?:\.\d+)?) km/h rpm=(\d+) coolant=(-?\d+) C"
)
DOOR_NAMES = ("driver", "passenger", "rear_left", "rear_right", "trunk")


def parse_event(line: str) -> tuple[float, int, int] | None:
    """Parse the existing high-level VehicleData text output."""
    match = LEGACY_EVENT_PATTERN.search(line)
    return (float(match[1]), int(match[2]), int(match[3])) if match else None


def parse_bridge_line(line: str) -> tuple[str, object] | None:
    """Parse only the bridge's service-level records, ignoring vSomeIP logs."""
    if match := DATA_PATTERN.fullmatch(line.strip()):
        return "data", (float(match[1]), int(match[2]), int(match[3]))
    if match := BODY_PATTERN.fullmatch(line.strip()):
        flags = int(match[2])
        return ("body_event" if match[1] == "BODY_EVENT" else "body_response"), flags
    if match := ACK_PATTERN.fullmatch(line.strip()):
        return "ack", match[1]
    if line.strip() == "DASHBOARD SERVICE available":
        return "service", True
    if line.strip() == "DASHBOARD SERVICE unavailable":
        return "service", False
    if line.startswith("DASHBOARD ERROR "):
        return "error", line.strip()[len("DASHBOARD ERROR "):]
    return None


class VehicleFeed:
    def __init__(self, profile: str) -> None:
        self.profile = profile
        self.lock = threading.Lock()
        self.stop_event = threading.Event()
        self.process: subprocess.Popen[str] | None = None
        self.status = "Connecting"
        self.service_available = False
        self.data: tuple[float, int, int] | None = None
        self.data_at: float | None = None
        self.body_flags: int | None = None
        self.body_at: float | None = None
        self.updated_at: str | None = None
        self.count = 0
        self.pending_action: str | None = None
        self.pending_accepted = False
        self.pending_deadline: float | None = None
        self.command_acceptance = "No command"
        self.command_result = "—"
        self.thread = threading.Thread(target=self.run, daemon=True)

    def start(self) -> None:
        self.thread.start()

    def snapshot(self) -> dict[str, object]:
        with self.lock:
            now = time.monotonic()
            if self.pending_deadline is not None and now > self.pending_deadline:
                self.command_result = "Timed out waiting for state change" if self.pending_accepted else "Timed out waiting for acknowledgement"
                self.pending_action = None
                self.pending_deadline = None
            data_fresh = self.data_at is not None and now - self.data_at <= 3
            body_fresh = self.body_at is not None and now - self.body_at <= 3
            status = self.status
            if self.service_available and not data_fresh:
                status = "Connected; waiting for vehicle data"
            return {
                "status": status,
                "connected": self.service_available,
                "speed": self.data[0] if data_fresh and self.data else None,
                "rpm": self.data[1] if data_fresh and self.data else None,
                "coolant": self.data[2] if data_fresh and self.data else None,
                "body": {name: bool(self.body_flags & (1 << index)) for index, name in enumerate(DOOR_NAMES)}
                if body_fresh and self.body_flags is not None else None,
                "body_fresh": body_fresh,
                "updated_at": self.updated_at if data_fresh else None,
                "count": self.count,
                "profile": self.profile,
                "pending": self.pending_action is not None,
                "command_acceptance": self.command_acceptance,
                "command_result": self.command_result,
            }

    def request_door(self, action: str) -> tuple[int, str]:
        if action not in ("open", "close"):
            return 400, "Action must be open or close"
        with self.lock:
            if not self.service_available or self.process is None or self.process.stdin is None:
                return 503, "SOME/IP service unavailable"
            if self.pending_action is not None:
                return 409, "A door command is still pending"
            self.pending_action = action
            self.pending_accepted = False
            self.pending_deadline = time.monotonic() + 6
            self.command_acceptance = "Waiting for gateway acknowledgement"
            self.command_result = "Waiting for BodyStatus"
            try:
                self.process.stdin.write(f"door 0 {action}\n")
                self.process.stdin.flush()
            except OSError:
                self.pending_action = None
                self.pending_deadline = None
                self.command_acceptance = "Connection lost"
                self.command_result = "Command not sent"
                return 503, "SOME/IP client disconnected"
        return 202, "Command sent; waiting for gateway acknowledgement"

    def handle_line(self, line: str) -> None:
        record = parse_bridge_line(line)
        if record is None:
            return
        kind, value = record
        now = time.monotonic()
        with self.lock:
            if kind == "data":
                self.data = value
                self.data_at = now
                self.updated_at = time.strftime("%H:%M:%S")
                self.count += 1
                self.status = "Live"
            elif kind in ("body_event", "body_response"):
                self.body_flags = value
                self.body_at = now
                if self.pending_action is not None and self.pending_accepted:
                    target_open = self.pending_action == "open"
                    if bool(value & 1) == target_open:
                        self.command_result = f"Confirmed {'open' if target_open else 'closed'} by BodyStatus"
                        self.pending_action = None
                        self.pending_deadline = None
            elif kind == "ack":
                if value == self.pending_action:
                    self.pending_accepted = True
                    self.pending_deadline = now + 6
                    self.command_acceptance = "Accepted for CAN transmission"
            elif kind == "service":
                self.service_available = bool(value)
                self.status = "Connected; waiting for data" if value else "Disconnected; reconnecting"
                if not value and self.pending_action is not None:
                    self.command_result = "Connection lost before confirmation"
                    self.pending_action = None
                    self.pending_deadline = None
            elif kind == "error" and self.pending_action is not None:
                self.command_acceptance = f"Rejected: {value}"
                self.command_result = "No state confirmation"
                self.pending_action = None
                self.pending_deadline = None

    def run(self) -> None:
        config_name = {"local": "client.json", "pc": "client_pc.json", "work": "client_office.json"}[self.profile]
        config = ROOT / "config" / "someip" / config_name
        client = ROOT / "build" / "cpp" / "someip" / "client" / "vehicle_dashboard_client"
        if not client.is_file() or not config.is_file():
            with self.lock:
                self.status = "Missing client build or SOME/IP config"
            return
        environment = os.environ.copy()
        environment["VSOMEIP_CONFIGURATION"] = str(config)
        environment["VSOMEIP_APPLICATION_NAME"] = "vehicle-client"
        while not self.stop_event.is_set():
            with self.lock:
                self.status = "Connecting"
                self.service_available = False
            try:
                process = subprocess.Popen(
                    [str(client)], cwd=ROOT, env=environment,
                    stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, text=True, bufsize=1,
                )
                with self.lock:
                    self.process = process
                assert process.stdout is not None
                for line in process.stdout:
                    if self.stop_event.is_set():
                        break
                    if os.environ.get("VEHICLE_DASHBOARD_DEBUG") == "1":
                        print(f"[SOME/IP] {line.rstrip()}", flush=True)
                    self.handle_line(line)
                if not self.stop_event.is_set():
                    process.wait()
            except OSError as error:
                with self.lock:
                    self.status = f"Client error: {error}"
                return
            finally:
                self.stop_process()
                with self.lock:
                    self.service_available = False
                    self.status = "Disconnected; reconnecting"
                    if self.pending_action is not None:
                        self.command_result = "Connection lost before confirmation"
                        self.pending_action = None
                        self.pending_deadline = None
            self.stop_event.wait(1)

    def stop_process(self) -> None:
        with self.lock:
            process = self.process
            self.process = None
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()

    def stop(self) -> None:
        self.stop_event.set()
        self.stop_process()
        self.thread.join(timeout=3)


class DashboardHandler(BaseHTTPRequestHandler):
    feed: VehicleFeed

    def send_json(self, status: int, data: dict[str, object]) -> None:
        body = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        path = urlparse(self.path).path
        if path == "/api/state":
            self.send_json(200, self.feed.snapshot())
            return
        if path != "/":
            self.send_error(404)
            return
        body = PAGE.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self) -> None:
        if urlparse(self.path).path != "/api/door":
            self.send_error(404)
            return
        origin = self.headers.get("Origin")
        if origin and origin != f"http://{self.headers.get('Host')}":
            self.send_json(403, {"message": "Invalid origin"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 1 <= length <= 256:
                raise ValueError("Invalid request length")
            payload = json.loads(self.rfile.read(length))
            action = payload["action"]
        except (ValueError, KeyError, TypeError, json.JSONDecodeError):
            self.send_json(400, {"message": "Expected JSON with action open or close"})
            return
        status, message = self.feed.request_door(action)
        self.send_json(status, {"message": message})

    def log_message(self, _format: str, *args: object) -> None:
        pass


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--someip-profile", choices=("local", "pc", "work"), default="work")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    feed = VehicleFeed(args.someip_profile)
    handler = type("VehicleDashboardHandler", (DashboardHandler,), {"feed": feed})
    server = ThreadingHTTPServer(("127.0.0.1", args.port), handler)
    feed.start()
    print(f"Dashboard: http://127.0.0.1:{args.port} (Ctrl+C to stop)", flush=True)
    try:
        server.serve_forever(poll_interval=0.2)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        feed.stop()


if __name__ == "__main__":
    main()
