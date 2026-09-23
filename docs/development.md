# Development

Linux/WSL is the primary development environment. Milestone 1 uses `vcan0`; the receiver and simulator accept a configurable interface name. Physical CAN testing is outside this milestone.

## Build and checks

Run these commands from the repository root:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 -m pytest tests/python
```

The sender requires `python-can`; Python tests require `pytest`. Use a Python environment with these packages available. C++ builds require vSomeIP 3 development files and Linux SocketCAN headers. CMake looks for the installed `vsomeip3` package and reports a clear error if it is unavailable.

## vCAN setup

On a Linux host with vCAN support and permission to manage network interfaces:

```sh
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set dev vcan0 up
ip -details link show vcan0
```

If `vcan0` already exists, skip the `ip link add` command. WSL2 kernel support and permissions must be checked on the actual development machine.

## Run the CAN slice

From the repository root, start the receiver in one terminal:

```sh
./build/cpp/can_gateway/socketcan_receiver --interface vcan0
```

Start the virtual ECU in another terminal:

```sh
PYTHONPATH=python python3 -m virtual_ecu --interface vcan0 --interval 1.0
```

Both default to `vcan0` when `--interface` is omitted. Press Ctrl+C to stop each process. For each valid `0x100` frame, the receiver prints a raw frame line followed by a line containing vehicle speed, RPM, and coolant temperature. The temporary layout is documented in [interfaces](interfaces.md).

To run the opt-in integration test after building:

```sh
python3 -m pytest tests/integration -m integration
```

## Run the local SOME/IP method slice

The provider and client have separate vSomeIP configurations. From the repository root, start the provider in one terminal:

```sh
VSOMEIP_CONFIGURATION="$PWD/config/someip/provider.json" ./build/cpp/someip/provider/vehicle_data_provider --demo
```

Then run the client in another terminal:

```sh
VSOMEIP_CONFIGURATION="$PWD/config/someip/client.json" ./build/cpp/someip/client/vehicle_data_client --timeout 5
```

`--demo` initializes `VehicleService` with 123.45 km/h, 2500 rpm, and 85 °C. Without it, the provider has no sample and methods return `E_NOT_READY`. The provider runs until stopped with Ctrl+C. The client exits after receiving all three replies or when its timeout expires. To run only the local SOME/IP integration test:

```sh
python3 -m pytest tests/integration/test_someip_local.py -m integration
```

These commands test the VehicleService-to-SOME/IP boundary with seeded data. The integrated live path uses the gateway below.

## Run the live gateway

With `vcan0` configured, start the integrated gateway in one terminal:

```sh
VSOMEIP_CONFIGURATION="$PWD/config/someip/provider.json" ./build/cpp/apps/vehicle_gateway --interface vcan0
```

Start the virtual ECU in a second terminal:

```sh
PYTHONPATH=python python3 -m virtual_ecu --interface vcan0 --interval 1.0
```

After the gateway prints a decoded vehicle-data line, request the latest values in a third terminal:

```sh
VSOMEIP_CONFIGURATION="$PWD/config/someip/client.json" ./build/cpp/someip/client/vehicle_data_client --timeout 5
```

The gateway starts without seeded values. A client request before the first valid CAN frame receives `E_NOT_READY`. Press Ctrl+C in the gateway and virtual ECU terminals to stop them. The gateway stops its CAN thread and vSomeIP runtime before exiting.

Run the live end-to-end test explicitly:

```sh
python3 -m pytest tests/integration/test_live_gateway.py -m integration
```
