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

Milestone 1B implements a local vCAN path from a Python virtual ECU to a C++ SocketCAN receiver, temporary signal decoder, and read-only vehicle service. Milestone 2A adds local vSomeIP method provider and client programs. Milestone 2B connects the live CAN path to that provider in one gateway process. Milestone 2C adds deterministic changing signals and VehicleData events. AI and dashboard features are not implemented yet.

The live data path is: virtual ECU → SocketCAN → CAN gateway → vehicle data → vehicle service → vSomeIP provider → client. A separate seeded provider remains available for local service tests. A high-level AI-facing API is a later step.

The vehicle service layer is the boundary between decoded CAN signals and SOME/IP services. The AI layer uses only high-level vehicle APIs; it does not handle raw CAN frames or SOME/IP messages.

See [project context](PROJECT_CONTEXT.md), [architecture](docs/architecture.md), and [development notes](docs/development.md) for the current plan.

## 한국어 프로젝트 요약

현재 구현 상태, 마일스톤, 검증 범위와 다음 단계를 [한국어 프로젝트 요약](docs/project_summary_ko.md)에서 확인할 수 있습니다.
