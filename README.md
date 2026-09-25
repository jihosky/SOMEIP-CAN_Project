# SOMEIP-CAN Project

## Quick Start

Build and configure vcan0 using the [Milestone 2C runbook](docs/runbook.md), then use two terminals:

~~~sh
./scripts/run_vehicle_server.sh --scenario acceleration
~~~

~~~sh
./scripts/run_vehicle_client.sh subscribe 3
~~~

For a single-command method smoke check, run ./scripts/run_demo.sh.

Milestone 1B implements a local vCAN path from a Python virtual ECU to a C++ SocketCAN receiver, temporary signal decoder, and read-only vehicle service. Milestone 2A adds local vSomeIP method provider and client programs. Milestone 2B connects the live CAN path to that provider in one gateway process. Milestone 2C adds deterministic changing signals and VehicleData events. A PC-side browser dashboard now displays VehicleData and BodyStatus events and controls the virtual driver door. AI features are not implemented yet.

The live data path is: virtual ECU → SocketCAN → CAN gateway → vehicle data → vehicle service → vSomeIP provider → client. A separate seeded provider remains available for local service tests. A high-level AI-facing API is a later step.

The vehicle service layer is the boundary between decoded CAN signals and SOME/IP services. The AI layer uses only high-level vehicle APIs; it does not handle raw CAN frames or SOME/IP messages.

See [project context](PROJECT_CONTEXT.md), [architecture](docs/architecture.md), and [development notes](docs/development.md) for the current plan.

## 한국어 프로젝트 요약

현재 구현 상태, 마일스톤, 검증 범위와 다음 단계를 [한국어 프로젝트 요약](docs/project_summary_ko.md)에서 확인할 수 있습니다.

## Live dashboard

For workplace Pi–PC operation, follow the [Korean command guide](docs/commands_ko.md).
Start the Pi server with `--someip-profile work`, then on PC/WSL run:

~~~sh
./scripts/run_vehicle_dashboard.sh --someip-profile work
~~~

Open `http://127.0.0.1:8765`. The dashboard shows vehicle values and five body
open/closed states. Driver door buttons show gateway acceptance separately from
BodyStatus confirmation. Home profiles remain `pi` on the server and `pc` on
the client. The dashboard uses Python's standard library and the built
`vehicle_dashboard_client` executable.
