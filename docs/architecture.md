# Architecture

## Milestone 1: local CAN signal path

```text
Python virtual ECU
       │ CAN frames
       ▼
configurable SocketCAN interface (vcan0 initially)
       │
       ▼
C++ SocketCAN receiver → raw frame console output
```

The virtual ECU publishes a temporary standard CAN frame on `vcan0`. The C++ receiver reads it through SocketCAN and prints its receive timestamp, identifier, DLC, and payload. This first slice does not decode signals. Keep the interface name configurable so `can0` or `can1` can replace `vcan0` later.

This slice ends at raw frame console output. Signal decoding, vehicle service, SOME/IP, AI, and the dashboard remain future work.

## Planned boundaries after Milestone 1

The CAN gateway owns raw frames and decoding. The `vehicle_service` layer will turn decoded signals into stable vehicle concepts and form the boundary presented to SOME/IP services. The SOME/IP provider and client will handle transport, while the AI agent will consume only a high-level vehicle API. Raw CAN frames and SOME/IP messages will not enter the AI layer. See [ADR 0002](decisions/0002-layer-boundaries.md).
