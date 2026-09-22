# Testing

Place C++ tests in `tests/cpp/`, Python unit tests in `tests/python/`, and cross-component tests in `tests/integration/`.

Future integration tests should use a configurable SocketCAN interface, initially `vcan0`, and verify the path from simulated signals through the vehicle service and SOME/IP client. Tests requiring vCAN or running middleware should be explicitly marked and run separately from unit tests.
