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

## Milestone 2A: local SOME/IP methods

```text
VehicleService → VehicleDataProvider → vSomeIP local routing → VehicleDataClient
```

The provider receives a `VehicleService` reference and serves three read-only methods. The client waits for service availability, requests each method, decodes the responses, and prints values. Shared IDs and the payload codec are in `cpp/someip/common/`. The provider and client use separate runtime configurations in `config/someip/`.

The Milestone 2A local integration test initializes `VehicleService` with deterministic values. The standalone receiver and seeded provider executables remain available for component checks.

## Milestone 2B: integrated gateway

```text
Python virtual ECU → vcan0 → SocketCanReceiver → SignalDecoder
                                                   │
                                                   ▼
                                             VehicleService
                                                   │
                                                   ▼
                                      VehicleDataProvider → vSomeIP → client
```

`vehicle_gateway` is the composition root. It owns one `SocketCanReceiver`, one decoder, one `VehicleService`, and one provider. The SocketCAN component converts Linux frames to transport-independent `CanFrame` values; it has no SOME/IP dependency. The provider reads only `VehicleService` and has no CAN dependency. No seeded values are used in this executable.

The CAN receive loop runs in a worker thread. vSomeIP's blocking `start()` runs on the main thread and invokes provider callbacks on its own runtime threads. `VehicleService` protects its latest-data snapshot with one mutex; updates and getters copy under that lock. A small control thread watches for SIGINT, SIGTERM, or CAN-loop failure, then stops the receiver and vSomeIP. The CAN socket has a 200 ms receive timeout so the worker can exit promptly; both threads are joined before process exit. No inter-process data transport is used inside the gateway.

## Boundaries for later milestones

The CAN gateway owns raw frames and decoding. The `vehicle_service` layer owns stable vehicle concepts and is the boundary presented to SOME/IP services. The SOME/IP provider and client handle transport, while the AI agent will consume only a high-level vehicle API. Raw CAN frames and SOME/IP messages will not enter the AI layer. See [ADR 0002](decisions/0002-layer-boundaries.md) and [ADR 0003](decisions/0003-use-vsomeip.md).
