"""Transmit a deterministic standard CAN frame over SocketCAN."""

import argparse
import json
from pathlib import Path
import time

import can


DEFINITION_PATH = Path(__file__).resolve().parents[2] / "config" / "can" / "milestone1.json"
DEFINITION = json.loads(DEFINITION_PATH.read_text())
BODY_DEFINITION = json.loads((DEFINITION_PATH.parent / "body.json").read_text())

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


def make_body_message(flags: int) -> can.Message:
    if not 0 <= flags < 128:
        raise ValueError("body status flags must fit in seven bits")
    return can.Message(
        arbitration_id=BODY_DEFINITION["can_id"],
        is_extended_id=False,
        data=bytes((flags,)),
    )


def apply_body_command(flags: int, message: can.Message) -> int | None:
    if (message.is_extended_id or message.arbitration_id != BODY_DEFINITION["command_can_id"]
            or len(message.data) != BODY_DEFINITION["command_dlc"]):
        return None
    door, open_value = message.data
    if door > BODY_DEFINITION["trunk_open_bit"] or open_value > 1:
        return None
    mask = 1 << door
    return (flags | mask) if open_value else (flags & ~mask)


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
            body_flags = 0
            while True:
                bus.send(make_message(samples[index % len(samples)]))
                bus.send(make_body_message(body_flags))
                index += 1
                deadline = time.monotonic() + args.interval
                while True:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        break
                    command = bus.recv(timeout=remaining)
                    if command is None:
                        break
                    updated = apply_body_command(body_flags, command)
                    if updated is not None:
                        body_flags = updated
                        bus.send(make_body_message(body_flags))
    except KeyboardInterrupt:
        print("Stopped virtual ECU.")
    except (can.CanError, OSError) as error:
        parser.exit(1, f"SocketCAN error: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
