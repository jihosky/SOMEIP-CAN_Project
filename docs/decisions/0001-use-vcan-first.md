# ADR 0001: Use vCAN for initial CAN development

Status: Accepted

## Context

The project has a Raspberry Pi 5 and dual-channel CAN HAT, but no independent physical ECU or CAN node for reliable bus testing. Milestone 1 needs a repeatable path from a virtual ECU to decoded signal output on the Linux/WSL development machine.

## Decision

Use Linux `vcan0` as the first CAN interface. The simulator and receiver will take the interface name from configuration, not embed `vcan0` in their logic. CAN IDs and signal definitions will live in dedicated configuration or DBC files. Validate the full Milestone 1 path over vCAN before moving to physical CAN.

## Consequences

- Development and automated integration tests can run without a physical CAN node.
- vCAN exercises SocketCAN APIs and the decode path, but cannot validate physical bus timing, wiring, termination, transceiver behavior, or CAN HAT setup.
- Physical CAN verification on `can0` or `can1` remains a later task and may reveal hardware-specific issues.
- Creating `vcan0` requires Linux vCAN support and permission to manage network interfaces; WSL2 support must be confirmed on the target machine.
