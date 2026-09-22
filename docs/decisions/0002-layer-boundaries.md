# ADR 0002: Separate CAN, vehicle service, SOME/IP, and AI

Status: Accepted

## Context

The planned platform will connect CAN signals to network services and an AI diagnostic agent. Giving upper layers raw frames or transport messages would couple them to CAN IDs, signal encoding, and middleware details.

## Decision

- The CAN gateway owns SocketCAN frames, CAN IDs, and decoding. It emits named decoded signals.
- The `vehicle_service` layer owns the mapping from decoded signals to stable vehicle data and diagnostic concepts. It is the abstraction boundary between CAN and SOME/IP.
- The SOME/IP provider exposes vehicle service operations and events; the client handles transport on the PC side.
- A high-level vehicle API presents those operations to the AI agent. The AI layer depends on that API only and has no raw CAN or SOME/IP dependency.

Milestone 1 implements only the path through decoded signal output. The later boundaries are documented now to guide interfaces, not to introduce their implementations early.

## Consequences

- CAN IDs and transport changes stay below the vehicle service boundary.
- Service contracts can be tested independently from SocketCAN and SOME/IP adapters.
- The AI agent can be tested with a high-level API substitute and cannot interpret raw CAN frames directly.
- The additional boundaries require explicit contracts and mapping tests when those components are implemented.
