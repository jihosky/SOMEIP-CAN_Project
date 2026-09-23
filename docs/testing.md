# Testing

## Unit tests

The Python unit test in `tests/python/` checks that the virtual ECU builds the agreed standard frame and deterministic payload. The C++ tests in `tests/cpp/` check raw output formatting, signal decoding, vehicle service getters and concurrent snapshot reads, and SOME/IP payload encoding and decoding, including malformed lengths and boundary values. These tests need no CAN interface. Run them with `python3 -m pytest tests/python` and `ctest --test-dir build --output-on-failure` after building.

## Milestone 1 integration tests

The test in `tests/integration/` is marked `integration` and is excluded from the default pytest path. It starts the C++ receiver and Python sender on an available interface, then checks both the raw frame and decoded vehicle data output. Set `CAN_INTERFACE` to use an interface other than `vcan0`. Run it explicitly with `python3 -m pytest tests/integration -m integration` after building and configuring vCAN. The test skips when the interface or binary is missing; it does not create or remove network interfaces.

The separate `test_someip_local.py` integration test starts a provider with deterministic `VehicleService` values, runs the client, and checks all three human-readable results. It requires the local vSomeIP runtime but no CAN interface. The test uses provider and client configuration files under `config/someip/` and terminates the provider it starts. AI and dashboard tests belong to later milestones.

The Milestone 2B `test_live_gateway.py` test starts the integrated gateway and Python virtual ECU on a configured vCAN interface. It waits for a decoded live sample, runs the SOME/IP client, and checks speed, RPM, and coolant temperature. It sends SIGINT to the sender and gateway and fails if either hangs during shutdown. This test is marked `integration` and requires vCAN plus local vSomeIP sockets; it does not use physical CAN or create network interfaces.

See [development](development.md) for build commands and vCAN setup.
