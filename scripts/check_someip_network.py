#!/usr/bin/env python3
"""Check the selected host profile against current Linux addresses and SD route."""

import ipaddress
import json
import subprocess
import sys
from pathlib import Path


def check(config_path: Path) -> None:
    config = json.loads(config_path.read_text())
    address = ipaddress.IPv4Address(config["unicast"])
    if address.is_loopback:
        return
    device = config["device"]
    prefix = ipaddress.IPv4Network(f"0.0.0.0/{config['netmask']}").prefixlen
    expected = f"{address}/{prefix}"
    shown = subprocess.run(
        ["ip", "-o", "-4", "addr", "show", "dev", device],
        capture_output=True, text=True, check=False,
    )
    if shown.returncode or not any(
        fields[2:4] == ["inet", expected]
        for line in shown.stdout.splitlines()
        if len(fields := line.split()) >= 4
    ):
        raise ValueError(f"Profile {config_path.name} requires {expected} on {device}; inspect ip addr show dev {device}")
    multicast = config["service-discovery"]["multicast"]
    route = subprocess.run(
        ["ip", "route", "get", multicast],
        capture_output=True, text=True, check=False,
    )
    if route.returncode or not any(
        fields[index:index+2] == ["dev", device]
        for line in route.stdout.splitlines()
        for fields in [line.split()]
        for index in range(len(fields)-1)
    ):
        raise ValueError(
            f"SD multicast route is not {device}; inspect ip route get {multicast}. "
            f"If appropriate: sudo ip route replace {multicast}/32 dev {device}"
        )


if __name__ == "__main__":
    try:
        check(Path(sys.argv[1]))
    except (IndexError, OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"[SETUP] {error}", file=sys.stderr)
        raise SystemExit(1) from error
