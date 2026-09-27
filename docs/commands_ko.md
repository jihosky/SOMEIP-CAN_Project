# 실행 명령어

프로젝트 루트에서 실행한다. Pi와 PC/WSL은 각자 **별도의 터미널과 저장소**를
사용한다. Pi 서버는 한 번에 하나만 실행한다.
`vehicle_gateway is already running`이 표시되면 기존 서버 터미널에서
Ctrl+C로 종료한 뒤 다시 시작한다. 기존 서버를 계속 사용할 경우에는
새 명령을 실행하지 않는다. 확인 명령은 다음과 같다.

~~~sh
pgrep -af 'run_vehicle_server.sh|vehicle_gateway|virtual_ecu'
ss -lntp '( sport = :30540 )'
~~~

기존 서버 터미널을 찾을 수 없거나 Ctrl+C 뒤에도 gateway가 남으면 Pi에서
다음 명령으로 **이 프로젝트의 gateway와 launcher**를 종료한다.

~~~sh
./scripts/stop_vehicle_server.sh
pgrep -af 'run_vehicle_server.sh|vehicle_gateway|virtual_ecu'
~~~

정상 종료 시 런처는 `Stopping server processes`와
`Server processes stopped`를 출력한다. 다른 터미널의 서버가 실행 중이면
현재 터미널에서 Ctrl+C를 눌러도 그 서버는 종료되지 않는다.

## Pi: 현재 고정 IP (`eth0=192.168.137.2`)

먼저 현재 주소와 SOME/IP multicast 경로를 확인한다.

~~~sh
cd ~/Portfolio/SOMEIP-CAN_Project
ip -4 addr show dev eth0
ip route get 224.244.224.245
ip link show vcan0
~~~

`provider_pi.json`의 `unicast`가 현재 `eth0` 주소와 같아야 하고,
multicast 경로는 `eth0`여야 한다. 경로가 다를 때만 다음 임시 경로를
적용한다.

~~~sh
sudo ip route replace 224.244.224.245/32 dev eth0
~~~

빌드 후 서버를 실행한다. 서버 터미널은 계속 열어 둔다.

~~~sh
cmake -S . -B build -G Ninja
cmake --build build
./scripts/run_vehicle_server.sh --someip-profile work --scenario acceleration --interface vcan0 --interval 1.0
~~~

Pi의 다른 터미널에서 포트와 송신 패킷을 확인할 수 있다.

~~~sh
ss -lntp '( sport = :30540 )'
ss -lunp '( sport = :30490 )'
sudo timeout 10 tcpdump -ni eth0 -vv 'udp port 30490 or tcp port 30540'
candump vcan0
~~~

`candump`는 종료할 때 Ctrl+C를 누른다. 서버도 서버 터미널에서 Ctrl+C로
종료한다. `tcpdump`의 `timeout` 종료 코드 124는 시간 제한 종료를 뜻한다.

## Pi: 집 (`eth0=192.168.50.2`)

집에서는 먼저 주소를 확인하고, `provider.json`의 `192.168.50.2`를 사용하는
`home` 프로필로 서버를 실행한다. multicast 경로가 다른 인터페이스를
가리킬 때만 위의 임시 경로 명령을 적용한다.

~~~sh
cd ~/Portfolio/SOMEIP-CAN_Project
ip -4 addr show dev eth0
ip route get 224.244.224.245
./scripts/run_vehicle_server.sh --someip-profile home --scenario acceleration --interface vcan0 --interval 1.0
~~~

## PC/WSL: 주소 확인과 빌드

현재 `client_pc.json`의 `unicast`는 집 주소 `192.168.50.1`이다. 근무지에서는
이 값을 그대로 사용하지 않는다. **WSL 자체가 가진 주소**와 Pi로 가는 경로를
확인하고, PC에서 별도의 근무지 프로필을 준비한 뒤 명령을 실행한다.
Windows Ethernet 주소만 확인해서 WSL의 주소라고 가정하지 않는다.

~~~sh
cd ~/Portfolio/SOMEIP-CAN_Project
ip -4 addr
ip route get 192.168.137.2
ip route get 224.244.224.245
ping -c 3 192.168.137.2
cmake -S . -B build -G Ninja
cmake --build build
~~~

아래의 `pc` 프로필은 **PC/WSL의 실제 주소로 설정을 맞춘 후에만** 사용한다.
집에서는 Pi 주소 `192.168.50.2`로 경로를 확인한다.

~~~sh
./scripts/run_vehicle_client.sh --someip-profile pc method
./scripts/run_vehicle_client.sh --someip-profile pc subscribe 3
./scripts/run_vehicle_client.sh --someip-profile pc body
./scripts/run_vehicle_client.sh --someip-profile pc body-subscribe 3
./scripts/run_vehicle_client.sh --someip-profile pc door 0 open
./scripts/run_vehicle_client.sh --someip-profile pc door 0 close
~~~

문 인덱스는 `0` 운전석, `1` 조수석, `2` 뒷좌석 왼쪽, `3` 뒷좌석 오른쪽,
`4` 트렁크다. 명령 접수 뒤 BodyStatus 이벤트에서 목표 상태를 확인한다.
PC에서 패킷을 함께 확인하려면 다른 터미널에서 다음을 실행한다.

~~~sh
sudo timeout 15 tcpdump -ni any -vv 'udp port 30490 or tcp port 30540'
~~~

프로토콜의 ID와 상태 비트는 [body_control_protocol.md](body_control_protocol.md),
PC Codex에 전달할 전체 요구사항은 [pc_handoff_body_ko.md](pc_handoff_body_ko.md)에
정리했다. Pi에서의 로컬 왕복은 검증됐지만 PC/WSL과 GUI 통신은 아직 검증되지
않았다.
