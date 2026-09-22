# Architecture

## Milestone 1B: local vehicle data path

```text
Python virtual ECU
       │ CAN frames
       ▼
configurable SocketCAN interface (vcan0 initially)
       │
       ▼
C++ SocketCAN receiver → CanFrame → SignalDecoder → VehicleData
                                                    │
                                                    ▼
                                               VehicleService
                                                    │
                                                    ▼
                                            vehicle data output
```

The virtual ECU publishes a temporary standard CAN frame on `vcan0`. The receiver owns Linux socket handling and translates each accepted frame into a `CanFrame` with an ID, DLC, payload, and receive timestamp. It prints the raw frame for inspection, then passes the domain frame to `SignalDecoder`. The decoder uses the temporary definition in `config/can/milestone1.json` and returns `VehicleData` for a valid `0x100` frame. `VehicleService` stores the latest decoded data and exposes read-only getters. The receiver prints that vehicle data to the console.

`CanFrame`, `VehicleData`, and `VehicleService` use no SocketCAN or SOME/IP types. The receiver is the composition point; vehicle decoding rules are in the decoder. This slice ends at vehicle data console output. SOME/IP, AI, and the dashboard remain future work.

## Planned boundaries after Milestone 1B

The CAN gateway owns raw frames and decoding. The `vehicle_service` layer owns stable vehicle concepts and will be the boundary presented to SOME/IP services. The SOME/IP provider and client will handle transport, while the AI agent will consume only a high-level vehicle API. Raw CAN frames and SOME/IP messages will not enter the AI layer. See [ADR 0002](decisions/0002-layer-boundaries.md).
