# Testing

## Unit tests

The Python unit test in `tests/python/` checks that the virtual ECU builds the agreed standard frame and deterministic payload. The C++ tests in `tests/cpp/` check raw output formatting, valid and invalid decoding, minimum and maximum signal values, timestamp propagation, and empty and updated vehicle service getters. These tests need no CAN interface. Run them with `python3 -m pytest tests/python` and `ctest --test-dir build --output-on-failure` after building.

## Milestone 1 integration tests

The test in `tests/integration/` is marked `integration` and is excluded from the default pytest path. It starts the C++ receiver and Python sender on an available interface, then checks both the raw frame and decoded vehicle data output. Set `CAN_INTERFACE` to use an interface other than `vcan0`. Run it explicitly with `python3 -m pytest tests/integration -m integration` after building and configuring vCAN. The test skips when the interface or binary is missing; it does not create or remove network interfaces.

This slice ends at vehicle data output. SOME/IP, AI, and dashboard tests belong to later milestones.

See [development](development.md) for build commands and vCAN setup.
