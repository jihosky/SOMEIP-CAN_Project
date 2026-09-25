"""Dashboard keeps command acceptance separate from observed body state."""

import io
from types import SimpleNamespace
import time

from vehicle_dashboard.__main__ import VehicleFeed, parse_bridge_line, parse_event


def test_parse_service_records():
    assert parse_bridge_line("DASHBOARD DATA speed=60.00 rpm=2400 coolant=72\n") == (
        "data", (60.0, 2400, 72)
    )
    assert parse_bridge_line("DASHBOARD BODY_EVENT flags=1\n") == ("body_event", 1)
    assert parse_bridge_line("DASHBOARD BODY_RESPONSE flags=0\n") == ("body_response", 0)
    assert parse_bridge_line("DASHBOARD ACK door=0 action=open\n") == ("ack", "open")
    assert parse_bridge_line("vSomeIP debug log") is None
    assert parse_event(
        "[CLIENT] VehicleData event: speed=60.00 km/h rpm=2400 coolant=72 C"
    ) == (60.0, 2400, 72)


def test_command_requires_ack_and_observed_status():
    feed = VehicleFeed("work")
    feed.handle_line("DASHBOARD SERVICE available")
    stream = io.StringIO()
    feed.process = SimpleNamespace(stdin=stream)
    feed.handle_line("DASHBOARD BODY_EVENT flags=0")
    assert feed.request_door("open")[0] == 202
    assert stream.getvalue() == "door 0 open\n"
    feed.handle_line("DASHBOARD BODY_EVENT flags=1")
    assert feed.snapshot()["pending"]  # A status before acknowledgement is insufficient.
    feed.handle_line("DASHBOARD ACK door=0 action=open")
    assert feed.snapshot()["command_acceptance"] == "Accepted for CAN transmission"
    assert feed.snapshot()["pending"]
    feed.handle_line("DASHBOARD BODY_EVENT flags=1")
    state = feed.snapshot()
    assert not state["pending"]
    assert state["body"]["driver"] is True
    assert state["command_result"] == "Confirmed open by BodyStatus"


def test_timeout_and_disconnect_clear_pending_command():
    feed = VehicleFeed("work")
    feed.handle_line("DASHBOARD SERVICE available")
    feed.process = SimpleNamespace(stdin=io.StringIO())
    assert feed.request_door("close")[0] == 202
    feed.pending_deadline = time.monotonic() - 1
    assert "Timed out" in feed.snapshot()["command_result"]
    assert not feed.snapshot()["pending"]
    assert feed.request_door("open")[0] == 202
    feed.handle_line("DASHBOARD SERVICE unavailable")
    state = feed.snapshot()
    assert not state["connected"]
    assert not state["pending"]
    assert state["command_result"] == "Connection lost before confirmation"
    assert feed.request_door("open")[0] == 503
