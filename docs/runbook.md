# Milestone 2C runbook

Run these commands on Linux or WSL from the repository root. The examples use /home/jiho/Portfolio/SOMEIP-CAN_Project; change that path if your checkout is elsewhere. The two-terminal launchers are the preferred manual check. The original five-terminal procedure remains below for debugging.

## Prerequisites and clean build

You need Python 3.10 or newer with venv and pip, python-can 4.x or 5.x, CMake 3.20 or newer, Ninja, a C++17 compiler, Linux SocketCAN headers, iproute2, kmod, and can-utils (candump). You also need the vSomeIP 3 development library, headers, CMake package, and runtime installed where CMake can find them. The current WSL setup has vSomeIP 3.7.6 under /usr/local.

From a clean checkout:

~~~sh
cd /home/jiho/Portfolio/SOMEIP-CAN_Project
python3 -m venv .venv
./.venv/bin/python -m pip install 'python-can>=4,<6' 'pytest>=8,<10'
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
~~~

The build produces build/cpp/apps/vehicle_gateway and build/cpp/someip/client/vehicle_data_client. Check the installed tools and vSomeIP package if setup fails:

~~~sh
command -v cmake ninja c++ ip candump
pkg-config --modversion vsomeip3
./.venv/bin/python -c 'import can; print(can.__version__)'
~~~

The commands below use .venv explicitly, so shell activation is unnecessary. pytest is for optional automated checks; it is not needed to operate the programs.

## Quick verification: two terminals

Build the project using the commands above. The launchers use .venv/bin/python when available, otherwise python3; that interpreter must have python-can installed. vcan0 must already exist and be up. The server launcher checks it and prints the exact sudo setup commands if it is missing; it never runs sudo itself.

**Terminal 1 — server stack:**

~~~sh
cd /home/jiho/Portfolio/SOMEIP-CAN_Project
./scripts/run_vehicle_server.sh --scenario acceleration
~~~

The launcher starts vehicle_gateway and the Python virtual ECU, records their PIDs, and prefixes each output line. Representative output:

~~~text
[SETUP] vcan0 available
[SETUP] Starting vehicle gateway...
[GATEWAY] Integrated vehicle gateway ready on vcan0
[SETUP] Starting Virtual ECU (acceleration scenario, 1.0s interval)...
[ECU] Sending ID=0x100 on vcan0 every 1s; press Ctrl+C to stop
[GATEWAY] 2026-09-23T12:00:00.000Z vehicle_speed_kph=20.00 engine_rpm=1200 coolant_temperature_c=70
~~~

**Terminal 2 — event subscription:**

~~~sh
cd /home/jiho/Portfolio/SOMEIP-CAN_Project
./scripts/run_vehicle_client.sh subscribe 3
~~~

Representative output includes all three acceleration values. Since the sender is already running when the client subscribes, the first received value can be any point in the repeating sequence:

~~~text
[CLIENT] Subscribed to VehicleData events
[CLIENT] VehicleData event: speed=20.00 km/h rpm=1200 coolant=70 C
[CLIENT] VehicleData event: speed=40.00 km/h rpm=1800 coolant=71 C
[CLIENT] VehicleData event: speed=60.00 km/h rpm=2400 coolant=72 C
~~~

The client exits after three events. To check the method API while Terminal 1 remains running, run this in Terminal 2:

~~~sh
./scripts/run_vehicle_client.sh method
~~~

It prints Vehicle speed, Engine RPM, and Coolant temperature. Each request reads the latest sample, so the three method values can come from different points in the acceleration sequence.

For fixed method values, stop Terminal 1 with Ctrl+C and restart it with the steady scenario:

~~~sh
./scripts/run_vehicle_server.sh --scenario steady --interval 1.0 --interface vcan0
~~~

Then run ./scripts/run_vehicle_client.sh method again in Terminal 2. Expect 123.45 km/h, 2500 rpm, and 85 C. Server options are --scenario steady|acceleration, --interval with a finite positive number of seconds, and --interface with a vCAN interface name. The defaults are steady, 1.0 seconds, and vcan0. The client supports method, subscribe, and subscribe COUNT (default count: 3). A failed client reports service unavailable, timeout, or request failure and exits nonzero.

Press Ctrl+C in Terminal 1 to stop the launcher. It sends termination signals only to the ECU and gateway PIDs it started, waits for them, and falls back to SIGKILL if either fails to exit within about three seconds. The client exits on its own after replies, the event count, or its timeout. The launcher leaves vcan0 up.

## Quick Demo

For a one-command smoke check, stop any manually running gateway/client and run:

~~~sh
cd /home/jiho/Portfolio/SOMEIP-CAN_Project
./scripts/run_demo.sh
~~~

The demo starts a steady server, retries method requests until the service is ready, verifies the three fixed values, then stops its server and children. Representative ending:

~~~text
[CLIENT] Vehicle speed: 123.45 km/h
[CLIENT] Engine RPM: 2500 rpm
[CLIENT] Coolant temperature: 85 C
[DEMO] PASS: Virtual ECU -> vcan -> gateway -> VehicleService -> SOME/IP client
~~~

It needs the same built binaries, Python environment, vSomeIP runtime, and preconfigured vcan0 as the two-terminal workflow. Ctrl+C during the demo also stops its server stack. A missing vcan0 or build artifact causes a nonzero exit and a setup message.

## Advanced: five-terminal debugging

The steps below expose the individual processes and CAN frames when you need to inspect each layer.

### Terminal 1: create and monitor vCAN

On a system where vcan0 does not already exist:

~~~sh
cd /home/jiho/Portfolio/SOMEIP-CAN_Project
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set dev vcan0 up
ip -details link show vcan0
candump vcan0
~~~

If vcan0 already exists, skip ip link add; run sudo ip link set dev vcan0 up if it is down. Leave candump running. A steady frame looks like this (spacing varies):

~~~text
vcan0  100   [8]  39 30 C4 09 7D 00 00 00
~~~

### Steady scenario: verify the three methods

**Terminal 3 — gateway:** Run the integrated gateway:

~~~sh
cd /home/jiho/Portfolio/SOMEIP-CAN_Project
VSOMEIP_CONFIGURATION="$PWD/config/someip/provider.json" ./build/cpp/apps/vehicle_gateway --interface vcan0
~~~

Wait for Integrated vehicle gateway ready on vcan0. The gateway starts without a sample, so an early method call can return E_NOT_READY.

**Terminal 2 — virtual ECU:** Start the steady sender:

~~~sh
cd /home/jiho/Portfolio/SOMEIP-CAN_Project
PYTHONPATH=python ./.venv/bin/python -m virtual_ecu --interface vcan0 --interval 1.0 --scenario steady
~~~

Its banner is similar to Sending ID=0x100 on vcan0 every 1s; press Ctrl+C to stop. Terminal 1 should show the 39 30 C4 09 7D 00 00 00 frame repeatedly. Terminal 3 should then show lines like:

~~~text
2026-09-23T12:00:00.000Z vehicle_speed_kph=123.45 engine_rpm=2500 coolant_temperature_c=85
~~~

The timestamp is illustrative; check the three values.

**Terminal 4 — method client:** After a decoded line appears in Terminal 3, run:

~~~sh
cd /home/jiho/Portfolio/SOMEIP-CAN_Project
VSOMEIP_CONFIGURATION="$PWD/config/someip/client.json" ./build/cpp/someip/client/vehicle_data_client --timeout 5
~~~

The client exits after all three responses. Their order may vary:

~~~text
Vehicle speed: 123.45 km/h
Engine RPM: 2500 rpm
Coolant temperature: 85 C
~~~

These lines verify GetVehicleSpeed, GetEngineRpm, and GetCoolantTemperature, respectively. vSomeIP may print additional configuration or routing logs.

### Acceleration scenario: verify event subscription

Stop the steady ECU in **Terminal 2** with Ctrl+C. Stop the gateway in **Terminal 3** with Ctrl+C and wait for it to exit. Restart the gateway using the same Terminal 3 command above. This gives the event check a fresh service without steady frames in flight. Terminal 4's method client has already exited.

**Terminal 5 — event client:** Start this before the acceleration ECU:

~~~sh
cd /home/jiho/Portfolio/SOMEIP-CAN_Project
VSOMEIP_CONFIGURATION="$PWD/config/someip/client.json" ./build/cpp/someip/client/vehicle_data_client --timeout 10 --subscribe 3
~~~

Wait until it prints Subscribed to VehicleData events. The client has requested event 0x8001 and subscribed to event group 0x0001. It exits after three events or at the timeout.

**Terminal 2 — acceleration ECU:** Start the changing signal source:

~~~sh
cd /home/jiho/Portfolio/SOMEIP-CAN_Project
PYTHONPATH=python ./.venv/bin/python -m virtual_ecu --interface vcan0 --interval 0.5 --scenario acceleration
~~~

Terminal 1 should now show this repeating frame sequence:

~~~text
vcan0  100   [8]  D0 07 B0 04 6E 00 00 00
vcan0  100   [8]  A0 0F 08 07 6F 00 00 00
vcan0  100   [8]  70 17 60 09 70 00 00 00
~~~

Terminal 3 should show decoded speed/RPM/temperature samples 20.00/1200/70, 40.00/1800/71, and 60.00/2400/72. Terminal 5 should print the three matching notifications in order:

~~~text
Subscribed to VehicleData events
VehicleData event: speed=20.00 km/h rpm=1200 coolant=70 C
VehicleData event: speed=40.00 km/h rpm=1800 coolant=71 C
VehicleData event: speed=60.00 km/h rpm=2400 coolant=72 C
~~~

The scenario repeats these three samples. The event client exits after the first three it receives. To watch longer, restart it with a larger --subscribe count and a longer --timeout. Method mode remains available while acceleration runs; its three method responses can come from different samples because each reads the latest value at request time.

### Shutdown

After the event client exits, press Ctrl+C in Terminal 2 to stop the ECU, then Ctrl+C in Terminal 3 to stop the gateway, and finally Ctrl+C in Terminal 1 to stop candump. The ECU closes its python-can bus. The gateway stops its CAN thread and vSomeIP application before exiting. The method client exits after its replies; the event client exits after its requested count or timeout. Leave vcan0 up for later tests. To remove it after all processes stop, run sudo ip link delete dev vcan0.

## Troubleshooting

| Symptom | Check and action |
| --- | --- |
| vcan0 missing in server launcher | The script prints the sudo modprobe and ip link commands; run them interactively with network administration permission. The launcher does not invoke sudo. |
| Build artifact missing | Run cmake -S . -B build -G Ninja followed by cmake --build build. The launcher checks the exact gateway/client executable path before starting. |
| Virtual ECU cannot start | Check the Python selected in the [SETUP] log and install python-can there. The launcher prefers .venv/bin/python, then python3. |
| vcan0 missing | Run ip link show vcan0. Load vcan with sudo modprobe vcan, then create and bring up the interface using Terminal 1's commands. On WSL, confirm the kernel supports vCAN. |
| Permission denied creating vcan0 | ip link add requires network administration privileges. Run the setup commands with sudo as an authorized user. |
| CMake cannot find vSomeIP | Check pkg-config --modversion vsomeip3 and locate vsomeip3Config.cmake. If installed outside CMake's search paths, configure with -Dvsomeip3_DIR=/path/to/cmake/vsomeip3. The development package is required. |
| VehicleDataService unavailable before timeout | Start the gateway first, check both VSOMEIP_CONFIGURATION paths, allow Service Discovery to complete, and confirm that the gateway remains running. |
| Method client reports E_NOT_READY | The gateway has no valid CAN sample yet. Start Terminal 2, check Terminal 1 for ID 100, wait for a decoded line in Terminal 3, then retry. |
| Service Discovery failure | Check both config/someip JSON files, multicast access to 224.244.224.245:30490, and local firewall rules. Review vSomeIP logs in the gateway and client terminals. |
| Subscription receives no events | Wait for Subscribed to VehicleData events before starting acceleration. Confirm new frames in candump and changing decoded lines in Terminal 3. Confirm event 0x8001 and group 0x0001 in the provider configuration. |
| Routing manager conflict | Only one process should own the configured vehicle-provider routing manager. Stop another vehicle_gateway or vehicle_data_provider with Ctrl+C. Inspect running processes with pgrep -af 'vehicle_gateway|vehicle_data_provider|vehicle_data_client'. |

For an automated check after the manual run, use ./.venv/bin/python -m pytest tests/integration/test_someip_events.py -m integration -q with vcan0 up and no manually running gateway or client.
