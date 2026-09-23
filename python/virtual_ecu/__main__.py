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
STEADY_SAMPLE = (VEHICLE_SPEED_KPH, ENGINE_RPM, COOLANT_TEMPERATURE_C)
ACCELERATION_SAMPLES = (
    (20.00, 1200, 70),
    (40.00, 1800, 71),
    (60.00, 2400, 72),
)


def make_payload(sample=STEADY_SAMPLE) -> bytes:
    payload = bytearray(DEFINITION["dlc"])
    speed, rpm, temperature = sample
    values = {
        "vehicle_speed": speed,
        "engine_rpm": rpm,
        "coolant_temperature": temperature,
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


def make_message(sample=STEADY_SAMPLE) -> can.Message:
    return can.Message(
        arbitration_id=DEFINITION["can_id"],
        is_extended_id=False,
        data=make_payload(sample),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--interface", default="vcan0", help="SocketCAN interface name")
    parser.add_argument(
        "--interval", type=float, default=1.0, help="seconds between frames (positive)"
    )
    parser.add_argument(
        "--scenario",
        choices=("steady", "acceleration"),
        default="steady",
        help="deterministic signal scenario",
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
            samples = (
                (STEADY_SAMPLE,)
                if args.scenario == "steady"
                else ACCELERATION_SAMPLES
            )
            index = 0
            while True:
                bus.send(make_message(samples[index % len(samples)]))
                index += 1
                time.sleep(args.interval)
    except KeyboardInterrupt:
        print("Stopped virtual ECU.")
    except (can.CanError, OSError) as error:
        parser.exit(1, f"SocketCAN error: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
