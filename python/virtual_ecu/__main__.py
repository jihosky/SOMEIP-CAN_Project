"""Transmit a deterministic standard CAN frame over SocketCAN."""

import argparse
import time

import can


TEST_CAN_ID = 0x100
TEST_PAYLOAD = bytes(range(8))


def make_message() -> can.Message:
    return can.Message(
        arbitration_id=TEST_CAN_ID,
        is_extended_id=False,
        data=TEST_PAYLOAD,
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
                f"Sending ID=0x{TEST_CAN_ID:03X} on {args.interface} "
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
