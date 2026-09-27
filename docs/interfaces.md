# Interfaces

## Temporary Milestone 1B CAN definition

The virtual ECU sends one standard (11-bit identifier) CAN data frame through the configured SocketCAN interface. Its temporary definition is in `config/can/milestone1.json`: ID `0x100`, DLC 8, and little-endian multi-byte values. The sender repeats the same frame at a configurable positive interval. The default interface is `vcan0`.

| Bytes | Signal | Raw encoding | Physical value |
| --- | --- | --- | --- |
| 0–1 | Vehicle speed | Unsigned 16-bit, little endian | Raw × 0.01 km/h |
| 2–3 | Engine RPM | Unsigned 16-bit, little endian | Raw rpm |
| 4 | Coolant temperature | Unsigned 8-bit | Raw − 40 °C |
| 5–7 | Reserved | Zero in the virtual ECU | Ignored by decoder |

The deterministic ECU values are 123.45 km/h, 2500 rpm, and 85 °C, yielding payload `39 30 C4 09 7D 00 00 00`. CMake reads the JSON definition to generate C++ constants; Python reads the same JSON file. Reconfigure CMake after changing the definition.

The C++ receiver reads standard CAN data frames and writes one line per frame to stdout:

```text
YYYY-MM-DDTHH:MM:SS.mmmZ ID=0x100 DLC=8 DATA=39 30 C4 09 7D 00 00 00
YYYY-MM-DDTHH:MM:SS.mmmZ vehicle_speed_kph=123.45 engine_rpm=2500 coolant_temperature_c=85
```

Both lines use the same receiver UTC timestamp, taken when the frame is processed, not a bus or ECU timestamp. The receiver ignores extended, remote, and error frames. The decoder ignores other CAN IDs or frames whose DLC is not 8. Errors go to stderr and yield a nonzero exit status.

`CanFrame` contains the ID, DLC, eight payload bytes, and receive timestamp without Linux types. `VehicleData` contains speed in km/h, RPM, coolant temperature in °C, and the same timestamp. `VehicleService` accepts decoded `VehicleData`, retains the latest sample, and exposes `getVehicleSpeed()`, `getEngineRpm()`, and `getCoolantTemperature()` as optional values. Before the first valid update, getters return no value. It does not expose raw frames.

`0x100` and its payload are temporary demonstration values. Permanent signal definitions may later move to DBC files. AI interfaces are not implemented in this milestone.

## VehicleDataService SOME/IP methods

Shared identifiers are defined in `cpp/someip/common/include/vehicle_someip/identifiers.hpp`. The service ID is `0x6301`, instance ID is `0x0001`, and the method IDs are listed below. Requests have an empty payload. Successful responses contain exactly the specified bytes in **big-endian (network) order**; these bytes are independent of the little-endian CAN layout.

| Method | ID | Response payload |
| --- | --- | --- |
| GetVehicleSpeed | `0x0001` | 4-byte unsigned integer, speed × 100 km/h |
| GetEngineRpm | `0x0002` | 2-byte unsigned integer, rpm |
| GetCoolantTemperature | `0x0003` | 2-byte signed two's-complement integer, °C |

For the local demo values (123.45 km/h, 2500 rpm, 85 °C), the payloads are `00 00 30 39`, `09 C4`, and `00 55`. When the service has no sample, the provider returns SOME/IP `E_NOT_READY` with no value payload. The client rejects incorrect payload lengths and reports unavailable service or response timeouts. Method responses do not include a timestamp yet.

In the integrated gateway, the first valid decoded CAN frame supplies the latest `VehicleData`; no initial sample is seeded. Requests arriving before that frame receive `E_NOT_READY`. Subsequent requests read the latest sample through `VehicleService`. The SOME/IP payload contract is unchanged from Milestone 2A.


## VehicleData event

The `VehicleData` event ID is `0x8001`; its event group ID is `0x0001`. Both identifiers are defined with the method identifiers in `cpp/someip/common/include/vehicle_someip/identifiers.hpp`. The client requests the event and subscribes to its event group after vSomeIP Service Discovery reports the service available.

The event payload is eight bytes in **big-endian (network) order**:

| Bytes | Value | Encoding |
| --- | --- | --- |
| 0–3 | Vehicle speed | Unsigned 32-bit, speed × 100 km/h |
| 4–5 | Engine RPM | Unsigned 16-bit rpm |
| 6–7 | Coolant temperature | Signed 16-bit two's-complement °C |

This payload contains vehicle-domain values and has no CAN identifier, DLC, reserved CAN bytes, or CAN byte order. Every successfully decoded `VehicleData` sample triggers one event notification. The method payloads and identifiers remain unchanged.

## VehicleState streaming (2026-09-27)

VehicleState is the domain meaning of the existing VehicleData event; existing
C++ names, CLI `VehicleData event:` output and wire contract stay compatible.
Service `0x6301`, instance `0x0001`, event `0x8001`, eventgroup `0x0001`.
Transport: reliable TCP 30540, unicast event delivery; SD uses UDP 30490 multicast
224.244.224.245. BodyStatus remains a separate event `0x8002`/group `0x0002`.

| Offset | Length | Encoding | Domain value |
| --- | --- | --- | --- |
| 0 | 4 | uint32 big-endian | speed in 0.01 km/h |
| 4 | 2 | uint16 big-endian | engine RPM |
| 6 | 2 | int16 two's complement big-endian | coolant Celsius |

Exactly eight bytes; null, short and oversized payloads are rejected. No CAN ID,
DLC, CAN layout, sequence number or source timestamp is transmitted. Speed uses
round-to-nearest hundredth; nonfinite, negative or overflowing values are rejected
by the encoder. Decoded VehicleData timestamp is default-initialized, not a source time.
Example 20 km/h, 1200 rpm, 70 C: `00 00 07 D0 04 B0 00 46`.

Every valid decoded CAN sample updates VehicleService and is passed as one coherent
domain snapshot to provider.publish(). notify(force=true) also publishes repeated
values: this is **sample-driven**, not value-change-driven or timer-driven.
No subscriber/GUI is needed to update the authoritative VehicleService. ET_EVENT
has no promised initial replay; a new subscriber waits for the next sample.
Methods remain independent on-demand reads. A three-method sequence need not be
one atomic snapshot, whereas each event contains one coherent sample.

Continuous `subscribe` requests the reliable event after application registration,
subscribes on availability and prints a success line only on the subscription-status
callback with status zero. The callback is application evidence, not a packet capture.
Non-notification/wrong-return-code messages are ignored; malformed notification
payloads exit cleanly with code 1. Finite `subscribe N` retains a 15-second overall
launcher deadline; continuous mode waits indefinitely, including before provider
startup. On service loss it reports unavailable and requests a subscription on
availability again; no replay, loss accounting or full fault recovery is promised.
SIGINT/SIGTERM stop the runtime through the client supervisor; no signal handler
calls vSomeIP directly. The 100ms supervisor wait is solely for lifecycle handling,
not polling vehicle data. Continuous launcher execs the client to preserve PID ownership.
