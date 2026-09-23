# ADR 0003: Use vSomeIP for vehicle service methods

Status: Accepted

## Context

Milestone 2A needs a C++ SOME/IP provider and client for the existing vehicle service boundary. vSomeIP 3.7.6 is installed in the Linux/WSL environment and exports the `vsomeip3` CMake target.

## Decision

Use vSomeIP for method request/response transport. Keep service and method identifiers in `cpp/someip/common/`, with payload encoding in a separate codec. The provider reads only `VehicleService`; it does not read CAN frames or decode CAN signals. The first configuration uses local host routing and no SOME/IP events or Service Discovery traffic.

## Consequences

- The provider and client can be built and tested locally without physical CAN.
- Builds now require the vSomeIP development package and fail clearly when its CMake package is absent.
- Binary payload formats are project contracts and need versioning before external clients depend on them.
- A later composition step must connect the live CAN process and provider to the same `VehicleService` instance. This milestone uses deterministic service data for its local integration test.
