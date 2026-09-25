# Body status and virtual door control

This extension uses the existing SOME/IP service `0x6301`, instance `0x0001`,
reliable TCP port `30540`. It does not change the existing vehicle data event
`0x8001` or its eight-byte payload.

| Interface | ID | Payload |
| --- | --- | --- |
| CAN body status | `0x200`, DLC 1 | One status bitmask byte |
| CAN door command | `0x201`, DLC 2 | Byte 0: door index; byte 1: `0` close, `1` open |
| SOME/IP GetBodyStatus | Method `0x0004` | Request empty; response: one status byte |
| SOME/IP SetDoor | Method `0x0005` | Request: same two command bytes; response: one byte `1` meaning accepted for vCAN transmission |
| SOME/IP BodyStatus event | Event `0x8002`, group `0x0002` | One status bitmask byte, reliable |

Status bits: 0 driver door open, 1 passenger door open, 2 rear-left door open,
3 rear-right door open, 4 trunk open, 5 doors locked, 6 headlights on. Bit 7
is reserved and rejected. Door indices 0 through 4 address the corresponding
door/trunk bits. A zero door bit means closed. The virtual ECU currently starts
with all bits zero and supports open/close commands for indices 0–4. Lock and
headlight bits are defined for future status input; no lock or light commands
are implemented yet.

The SetDoor response acknowledges that the gateway wrote the command to
`vcan0`. The changed `0x200` CAN status and `0x8002` SOME/IP event confirm
the virtual ECU applied it. The gateway exposes the remote command only when
the selected CAN interface starts with `vcan`; physical CAN interfaces do not
send body commands. The demo has no per-client authentication, so keep remote
control on the intended test network.

On the Pi at the workplace, with `eth0=192.168.137.69` and the SD multicast
route through eth0, start:

~~~sh
./scripts/run_vehicle_server.sh --someip-profile work --scenario acceleration
~~~

The client launcher offers `body`, `body-subscribe [COUNT]`, and
`door INDEX open|close` modes. `client_pc.json` retains the home address
`192.168.50.1`; `client_office.json` uses the workplace PC/WSL address
`192.168.137.1`. After checking the PC network, run:

~~~sh
./scripts/run_vehicle_client.sh --someip-profile work body
./scripts/run_vehicle_client.sh --someip-profile work body-subscribe 3
./scripts/run_vehicle_client.sh --someip-profile work door 0 open
./scripts/run_vehicle_client.sh --someip-profile work door 0 close
~~~

The Pi local round-trip was verified on 2026-09-25 with a temporary client
config using `192.168.137.69`. A `candump` capture showed `201#0001` followed
by `200#01`, then `201#0000` followed by `200#00`. The SOME/IP client printed
`driver_door=open` and `driver_door=closed`, and the existing speed/RPM/coolant
method client continued to return acceleration values. The PC/WSL workplace checks on 2026-09-26 also received both event types and
confirmed door open/close through the GUI API. See [Korean command guide](commands_ko.md) for commands and capture checks.
