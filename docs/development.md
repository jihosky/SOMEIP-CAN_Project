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

The sender requires `python-can`; Python tests require `pytest`. Use a Python environment with these packages available. The C++ receiver uses Linux SocketCAN headers and the C++17 standard library.

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
