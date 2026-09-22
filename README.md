# SOMEIP-CAN Project

Milestone 1B implements a local vCAN path from a Python virtual ECU to a C++ SocketCAN receiver, temporary signal decoder, and read-only vehicle service. SOME/IP, AI, and dashboard features are not implemented yet.

The current data path is: virtual ECU → SocketCAN → CAN gateway → vehicle data → vehicle service → console output. The later plan extends this through SOME/IP and a high-level vehicle API.

The vehicle service layer is the boundary between decoded CAN signals and SOME/IP services. The AI layer uses only high-level vehicle APIs; it does not handle raw CAN frames or SOME/IP messages.

See [project context](PROJECT_CONTEXT.md), [architecture](docs/architecture.md), and [development notes](docs/development.md) for the current plan.
