# SOMEIP-CAN Project

Initial skeleton for an AI-assisted automotive software platform. No vehicle features are implemented yet.

The planned data path is: virtual ECU → SocketCAN → CAN gateway → vehicle service layer → SOME/IP provider → SOME/IP client → high-level vehicle API → AI agent and dashboard.

The vehicle service layer is the boundary between decoded CAN signals and SOME/IP services. The AI layer uses only high-level vehicle APIs; it does not handle raw CAN frames or SOME/IP messages.

See [project context](PROJECT_CONTEXT.md), [architecture](docs/architecture.md), and [development notes](docs/development.md) for the current plan.
