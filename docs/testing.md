# Testing

## Unit tests

The Python unit test in `tests/python/` checks that the virtual ECU builds the agreed standard frame and deterministic payload. The C++ test in `tests/cpp/` checks console formatting without a CAN interface. Run them with `python3 -m pytest tests/python` and `ctest --test-dir build --output-on-failure` after building. Future decoding tests belong to a later slice.

## Milestone 1 integration tests

The test in `tests/integration/` is marked `integration` and is excluded from the default pytest path. It starts the C++ receiver and Python sender on an available interface, then checks that the receiver prints the expected ID, DLC, payload, and timestamp shape. Set `CAN_INTERFACE` to use an interface other than `vcan0`. Run it explicitly with `python3 -m pytest tests/integration -m integration` after building and configuring vCAN. The test skips when the interface or binary is missing; it does not create or remove network interfaces.

This slice ends at raw CAN frame output. DBC decoding, vehicle service, SOME/IP, AI, and dashboard tests belong to later slices.

See [development](development.md) for build commands and vCAN setup.
