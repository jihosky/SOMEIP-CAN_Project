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

## Raspberry Pi 제공자 → Ethernet → PC/WSL 클라이언트 검증

2026-09-23에 **Pi 제공자 → 직접 Ethernet → PC/WSL 클라이언트**를 실측했다. Pi 소유자가 `192.168.50.2:30540` LISTEN과 `eth0`의 UDP 30490 SD 송신을 확인했다. Windows가 Pi의 UDP 30490 패킷을 수신했고, WSL의 `vehicle-client`가 `0x6301/0x0001` 서비스를 발견하여 메서드 세 값과 가속 이벤트 세 개를 수신했다. 이번 검증은 직접 연결·이 주소 조합에서의 동작이며 장시간 안정성이나 다른 네트워크에서는 아직 확인하지 않았다.

로컬 기본 프로필은 `config/someip/provider.json`과 `client.json`으로 루프백 `127.0.0.1`을 사용한다. 멀티 호스트 프로필은 `provider_pi.json`의 `192.168.50.2/24` 및 `client_pc.json`의 `192.168.50.1/24`를 사용한다. 각 호스트의 `routing`은 해당 호스트의 애플리케이션(`vehicle-provider` 또는 `vehicle-client`)이다. WSL 실측 주소는 `eth0=192.168.50.1/24`이고 Windows Ethernet에도 같은 주소가 있다. WSL은 mirrored 네트워킹을 사용한다. 직접 Ethernet의 인터페이스 이름은 양쪽 모두 `eth0`였다. 다른 이름·주소를 쓰면 JSON의 `unicast`/`device`와 런처의 사전 검사를 함께 조정해야 한다.

실험 중 WSL의 SD 멀티캐스트 경로가 `eth0`에서 `eth1`로 되돌아가고, Pi ping이 일시적으로 끊긴 적이 있다. 링크가 재설정된 뒤에는 경로와 Pi 제공자 상태를 **클라이언트 실행 직전 다시 확인**한다. Windows Public 및 WSL Hyper-V의 기본 인바운드는 `Block`이었다. Pi 주소와 UDP 30490에 한정한 아래 두 허용 규칙을 적용한 후 Windows에서 SD 패킷 수신과 WSL 메서드·이벤트가 성공했다.

**선행 조건:** Pi와 PC/WSL 모두 이 저장소의 같은 버전, C++17 컴파일러, CMake 3.20 이상, Ninja, Linux SocketCAN 헤더, vSomeIP 3 개발 파일과 런타임, `ip`, `ss`, `tcpdump`가 필요하다. Pi에는 Python 3.10 이상, `python-can`, `can-utils`, `vcan` 커널 지원이 필요하다. PC/WSL에도 빌드 및 자동 테스트용 Python과 `pytest`를 준비한다. Pi와 PC의 네트워크 관리자 또는 방화벽이 UDP 30490 멀티캐스트와 TCP 30540을 허용해야 한다. 현재 mirrored WSL에서 수신을 확인했지만 다른 Windows/WSL 버전이나 방화벽 정책에서는 다시 검증해야 한다.

**두 호스트 각각에서 저장소 루트로 이동해 빌드:**

~~~sh
python3 -m venv .venv
./.venv/bin/python -m pip install 'python-can>=4,<6' 'pytest>=8,<10'
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
pkg-config --modversion vsomeip3
~~~

`find_package(vsomeip3 CONFIG)`가 실패하면 해당 호스트에 vSomeIP 개발 패키지/CMake 설정을 먼저 설치하거나 `-Dvsomeip3_DIR=...`로 위치를 지정한다. Pi의 가상 CAN 인터페이스가 아직 없을 때만 다음을 실행한다. 이 명령의 `sudo`는 사용자가 직접 실행하며 런처는 권한 상승을 수행하지 않는다.

~~~sh
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set dev vcan0 up
ip -details link show vcan0
~~~

`vcan0`가 이미 있으면 `ip link add`는 생략한다. 두 호스트에서 먼저 주소와 멀티캐스트 경로를 확인한다.

~~~sh
ip -brief -4 addr show dev eth0
ip route get 192.168.50.2
ip route get 224.244.224.245
ip maddr show dev eth0
~~~

Pi의 `eth0`에는 `192.168.50.2/24`, PC/WSL의 `eth0`에는 `192.168.50.1/24`가 있어야 한다. 두 호스트 모두 `ip route get 224.244.224.245`에 `dev eth0`가 나타나야 한다. 다른 장치가 나오면 해당 호스트에서 의도적으로 멀티캐스트 경로를 설정하고 다시 확인한다.

~~~sh
sudo ip route replace 224.244.224.245/32 dev eth0
ip route get 224.244.224.245
~~~

이 경로 변경은 호스트 네트워크 설정이다. 필요하면 종료 후 `sudo ip route del 224.244.224.245/32 dev eth0`로 직접 되돌린다. 특히 PC/WSL에 Wi-Fi나 다른 기본 경로가 있으면 SD 멀티캐스트가 다른 장치로 나갈 수 있다. mirrored WSL에서는 링크 상태 변경 후 이 경로가 사라진 사례가 있으므로 클라이언트 실행 직전에 재검사한다.

**Pi 터미널 1 — 패킷 감시:**

~~~sh
sudo tcpdump -ni eth0 -vv 'udp port 30490 or tcp port 30540'
~~~

**Pi 터미널 2 — 가상 ECU와 제공자:**

~~~sh
./scripts/run_vehicle_server.sh --someip-profile pi --scenario acceleration --interface vcan0 --interval 1.0
~~~

시작 로그에 `config/someip/provider_pi.json`, `application: vehicle-provider`, `VSOMEIP_CONFIGURATION` 경로, `[SOMEIP] vehicle-provider registered`, `offer_service requested`, 그리고 `vehicle_speed_kph=20.00` 같은 디코딩 값을 확인한다. `Integrated vehicle gateway ready`는 프로세스 초기화 신호일 뿐 등록·OfferService 성공 증명이 아니다. vSomeIP의 `Using configuration file`, `OFFER`, SD/엔드포인트 로그도 확인한다.

**Pi 터미널 3 — 프로세스·바인딩 확인:**

~~~sh
pgrep -af 'vehicle_gateway|virtual_ecu'
ss -lntp '( sport = :30540 )'
ss -lunp '( sport = :30490 )'
ip maddr show dev eth0
~~~

`ss -lntp`에는 게이트웨이가 소유한 `192.168.50.2:30540` TCP `LISTEN`이 기대된다. SD가 정상 시작되면 UDP 30490 바인딩과 `eth0`의 멀티캐스트 가입도 확인한다. vSomeIP 버전·소켓 표시 방식에 따라 UDP 로컬 주소는 `0.0.0.0:30490` 또는 `192.168.50.2:30490`처럼 다를 수 있다. `sudo ss -lntup`로 소유 프로세스 표시를 보완한다. 런처의 게이트웨이 PID를 읽어 실제 프로세스 환경을 확인할 수도 있다.

~~~sh
tr '\0' '\n' < /proc/GATEWAY_PID/environ | grep -E '^VSOMEIP_(CONFIGURATION|APPLICATION_NAME)='
~~~

위 명령의 `GATEWAY_PID`를 런처 로그의 숫자로 바꾼다. 설정 파일이 실제로 쓰였는지는 환경 값과 **vSomeIP의 `Using configuration file` 로그를 함께** 확인한다.

**PC/WSL 터미널 1 — 패킷 감시:**

~~~sh
sudo tcpdump -ni eth0 -vv 'udp port 30490 or tcp port 30540'
~~~

**PC/WSL 터미널 2 — 클라이언트:**

~~~sh
./scripts/run_vehicle_client.sh --someip-profile pc subscribe 3
./scripts/run_vehicle_client.sh --someip-profile pc method
~~~

첫 명령에서 `config/someip/client_pc.json`, `application: vehicle-client`, `Subscribed to VehicleData events`, 그리고 가속 샘플 20/1200/70, 40/1800/71, 60/2400/72의 순환 순서에 맞는 세 이벤트가 기대된다. 클라이언트 시작 시점에 따라 첫 값은 세 샘플 중 하나일 수 있다. 두 번째 명령은 `Vehicle speed`, `Engine RPM`, `Coolant temperature`을 출력한다. 값은 각 요청 시점의 최신 샘플이므로 서로 다른 순환 지점에서 올 수 있다. 2026-09-23 실측 출력은 다음과 같다.

~~~text
[CLIENT] VehicleData event: speed=60.00 km/h rpm=2400 coolant=72 C
[CLIENT] VehicleData event: speed=20.00 km/h rpm=1200 coolant=70 C
[CLIENT] VehicleData event: speed=40.00 km/h rpm=1800 coolant=71 C
[CLIENT] Vehicle speed: 60.00 km/h
[CLIENT] Engine RPM: 2400 rpm
[CLIENT] Coolant temperature: 72 C
~~~

WSL 클라이언트 실행 중에는 `ss -lunp '( sport = :30490 )'`에서 `192.168.50.1%eth0:30490` 바인딩, `ss -ntp '( dport = :30540 )'`에서 `192.168.50.1:<임시 포트> → 192.168.50.2:30540 ESTAB`를 확인했다. 프로세스 환경은 `VSOMEIP_APPLICATION_NAME=vehicle-client`, `VSOMEIP_CONFIGURATION=.../config/someip/client_pc.json`이었다.

**패킷 판별:** Pi의 캡처에서 `192.168.50.2.30490 > 224.244.224.245.30490` 형태의 UDP SD 패킷이 주기적으로 나타나야 한다. PC의 캡처에도 이 OfferService 패킷이 보여야 한다. PC 클라이언트 시작 후에는 반대 방향의 SD FindService/SubscribeEventgroup 트래픽과 `192.168.50.1:<임시 포트> > 192.168.50.2.30540` TCP 연결이 기대된다. `tcpdump -XX -ni eth0 udp port 30490`로 SD 페이로드를 저장·검토할 수 있다. 단순 UDP 포트 표시만으로 OfferService 종류를 증명할 수 없으므로 vSomeIP `OFFER` 로그 또는 SOME/IP SD 디섹터로 메시지 종류를 확인한다. 이번 검증에서는 Pi 캡처의 UDP 30490 송신, Windows 소켓의 `192.168.50.2:30490`에서 온 56바이트 SD 수신, WSL vSomeIP의 `ON_OFFER_SERVICE`와 `ON_AVAILABLE`, TCP `ESTAB`, 메서드·이벤트 출력을 각각 확인했다. WSL `tcpdump`는 일반 사용자 권한에서 `Operation not permitted`였으므로 WSL의 원시 패킷 캡처는 수행하지 못했다.

공식 vSomeIP [설정 설명](https://github.com/COVESA/vsomeip/blob/master/documentation/vsomeipConfiguration.md)은 `unicast`, `device`, `netmask`, 서비스 포트와 SD 멀티캐스트의 의미를 정의한다. TCP 서비스 포트는 문서화된 `reliable: {"port": "30540"}` 형태로 설정한다. [멀티 호스트 예제 설명](https://github.com/COVESA/vsomeip/wiki/vsomeip-in-10-minutes)은 멀티캐스트 경로 확인을 강조한다.

**문제 구분:**

| 관찰 | 우선 확인 |
| --- | --- |
| 디코딩 값은 나오지만 `registered`가 없음 | vSomeIP 초기화·라우팅 매니저 충돌, 실제 설정 경로, 프로세스가 계속 실행 중인지, `debug` 로그 |
| `registered`/Offer 로그는 있으나 TCP 30540 `LISTEN` 없음 | `services`의 `0x6301/0x0001`과 `reliable: 30540`, `192.168.50.2` 바인딩, 포트 점유·오류 로그 |
| TCP는 열렸으나 Pi eth0에 UDP 30490 송신 없음 | Pi의 `ip route get 224.244.224.245`, `device`/unicast 주소, SD 활성화, UDP 바인딩·라우팅 로그, 다른 인터페이스의 `tcpdump -ni any udp port 30490` |
| Pi에는 SD가 보이지만 PC에는 없음 | Ethernet 멀티캐스트 전달, mirrored WSL과 Windows 방화벽, 양쪽 `ip maddr`와 경로. 일반 사용자 `tcpdump`는 권한 오류가 나므로 `sudo tcpdump`를 사용 |
| PC에 SD는 보이나 서비스 불가 | PC 설정 로드·라우팅 매니저, SD 구독, 클라이언트의 `192.168.50.1` 바인딩, TCP 30540 연결 |
| 다른 라우팅 매니저 실행 중 | `pgrep -af 'vehicle_gateway|vehicle_data_provider|vehicle_data_client'`로 확인하고 본인이 시작한 충돌 프로세스만 정상 종료 |

Windows mirrored WSL의 인바운드 정책은 관리자 권한이 없어도 읽기 전용 PowerShell에서 확인할 수 있다. 2026-09-23 처음에는 Windows Ethernet이 Public 프로필이고, 적용 중인 Public 및 WSL Hyper-V 인바운드 기본 동작이 모두 `Block`이었으며 UDP 30490 허용 규칙이 없었다. 같은 Pi 송신 조건에서 두 규칙을 추가한 후 Windows 수신과 WSL 요청·구독이 성공했다. 이는 이번 환경에서 인바운드 방화벽 설정이 SD 수신을 막았다는 **전후 비교 증거**다. 링크 및 멀티캐스트 경로의 일시적 변화도 별도로 관찰됐다.

~~~powershell
Get-NetFirewallProfile -PolicyStore ActiveStore -Name Public | Select-Object Name,Enabled,DefaultInboundAction
Get-NetFirewallHyperVVMSetting -PolicyStore ActiveStore -Name '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}' | Select-Object Name,Enabled,DefaultInboundAction
Get-NetFirewallHyperVRule -VMCreatorId '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}'
~~~

네트워크 관리자와 협의하여 필요할 때만 **관리자 PowerShell**에서 Pi 주소와 UDP 30490으로 한정한 규칙을 적용한다. 기존 규칙 이름이 없는지 먼저 확인한다.

~~~powershell
New-NetFirewallRule -DisplayName 'SOMEIP SD Pi to PC' -Direction Inbound -Action Allow -Profile Public -Protocol UDP -LocalAddress 192.168.50.1 -LocalPort 30490 -RemoteAddress 192.168.50.2
New-NetFirewallHyperVRule -Name 'SOMEIP-SD-WSL' -DisplayName 'SOMEIP SD Pi to WSL' -Direction Inbound -Action Allow -VMCreatorId '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}' -Protocol UDP -LocalPorts 30490 -RemoteAddresses 192.168.50.2
~~~

실험 후 규칙이 불필요하면 관리자 PowerShell에서 `Remove-NetFirewallRule -DisplayName 'SOMEIP SD Pi to PC'`와 `Remove-NetFirewallHyperVRule -Name 'SOMEIP-SD-WSL'`로 직접 제거한다. WSL의 `tcpdump`에는 별도로 Linux 관리자 권한이 필요하다.

방화벽은 `sudo nft list ruleset` 또는 사용 중인 방화벽 도구로 점검한다. 직접 연결 ping 성공은 유니캐스트 IP만 검증하며 SD 멀티캐스트 성공을 뜻하지 않는다. 종료할 때는 PC 클라이언트 완료를 확인한 뒤 Pi 터미널 2에서 Ctrl+C로 런처와 그 자식 ECU·게이트웨이를 종료하고, 마지막에 양쪽 `tcpdump`를 Ctrl+C로 멈춘다. `vcan0`와 수동으로 추가한 멀티캐스트 경로는 자동 제거되지 않는다.

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
