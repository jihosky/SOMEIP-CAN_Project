"""Transmit a deterministic standard CAN frame over SocketCAN."""

import argparse
import json
from pathlib import Path
import time

import can


DEFINITION_PATH = Path(__file__).resolve().parents[2] / "config" / "can" / "milestone1.json"
DEFINITION = json.loads(DEFINITION_PATH.read_text())

VEHICLE_SPEED_KPH = 123.45
ENGINE_RPM = 2500
COOLANT_TEMPERATURE_C = 85


def make_payload() -> bytes:
    payload = bytearray(DEFINITION["dlc"])
    values = {
        "vehicle_speed": VEHICLE_SPEED_KPH,
        "engine_rpm": ENGINE_RPM,
        "coolant_temperature": COOLANT_TEMPERATURE_C,
    }
    for name, physical_value in values.items():
        signal = DEFINITION[name]
        raw_value = round((physical_value - signal["offset"]) / signal["scale"])
        start = signal["start"]
        length = signal["length"]
        payload[start : start + length] = raw_value.to_bytes(
            length, DEFINITION["byte_order"]
        )
    return bytes(payload)


def make_message() -> can.Message:
    return can.Message(
        arbitration_id=DEFINITION["can_id"],
        is_extended_id=False,
        data=make_payload(),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--interface", default="vcan0", help="SocketCAN interface name")
    parser.add_argument(
        "--interval", type=float, default=1.0, help="seconds between frames (positive)"
    )
    args = parser.parse_args()
    if args.interval <= 0 or not 0 < args.interval < float("inf"):
        parser.error("--interval must be a finite positive number")

    try:
        with can.Bus(interface="socketcan", channel=args.interface) as bus:
            print(
                f"Sending ID=0x{DEFINITION['can_id']:03X} on {args.interface} "
                f"every {args.interval:g}s; press Ctrl+C to stop",
                flush=True,
            )
            while True:
                bus.send(make_message())
                time.sleep(args.interval)
    except KeyboardInterrupt:
        print("Stopped virtual ECU.")
    except (can.CanError, OSError) as error:
        parser.exit(1, f"SocketCAN error: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
