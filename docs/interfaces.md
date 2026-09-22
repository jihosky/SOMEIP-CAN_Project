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

`0x100` and its payload are temporary demonstration values. Permanent signal definitions may later move to DBC files. SOME/IP and AI interfaces are not implemented in this milestone.
