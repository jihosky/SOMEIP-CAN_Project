# Pi–PC SOME/IP 실행 명령 (한국어)

저장소 루트에서 실행한다. **근무지**는 Pi `192.168.137.69/24`, PC/WSL
`192.168.137.1/24`이며 각각 `provider_office.json`, `client_office.json`을
사용한다. **집**은 Pi `192.168.50.2/24`, PC/WSL `192.168.50.1/24`이며
`provider_pi.json`, `client_pc.json`을 사용한다. 두 환경의 각 JSON은
`unicast` 주소만 다르다. 주소가 바뀌면 먼저 `ip` 출력과 JSON을 확인한다.

## 1. 양쪽 빌드

~~~sh
cd ~/Portfolio/SOMEIP-CAN_Project
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./.venv/bin/python -m pytest tests/python -q
~~~

Pi 저장소의 본문 제어 변경은 2026-09-26 현재 미커밋 상태다. PC와 Pi 저장소는
자동 동기화되지 않으므로 각각 `git status --short`와
`docs/body_control_protocol.md`를 확인한다.

## 2. 근무지 네트워크 확인

PC/WSL에서:

~~~sh
ip -brief -4 addr
ip route get 192.168.137.69
ip route get 224.244.224.245
python3 scripts/check_someip_network.py config/someip/client_office.json
~~~

기대: WSL `eth0=192.168.137.1/24`, Pi 경로와 SD 멀티캐스트 경로가
모두 `dev eth0 src 192.168.137.1`. Pi에서는 `eth0=192.168.137.69/24`와
`ip route get 224.244.224.245`의 `dev eth0`를 확인한다. 경로가 다른 경우에만
해당 호스트에서 `sudo ip route replace 224.244.224.245/32 dev eth0`를 적용한다.

Windows mirrored WSL의 Public 및 Hyper-V 방화벽에서 Pi 주소
`192.168.137.69`의 UDP 30490을 허용해야 SD OfferService가 들어온다.
기존 규칙을 먼저 확인하고, 근무지 정책에 맞춰 관리자 PowerShell에서 필요한
규칙만 적용한다. 집 주소 `192.168.50.x` 규칙은 근무지 규칙을 대신하지 않는다.

~~~powershell
Get-NetFirewallRule -DisplayName 'SOMEIP SD Pi to PC' -ErrorAction SilentlyContinue
Get-NetFirewallHyperVRule -Name 'SOMEIP-SD-WSL' -ErrorAction SilentlyContinue
New-NetFirewallRule -DisplayName 'SOMEIP SD Pi to PC' -Direction Inbound -Action Allow -Profile Public -Protocol UDP -LocalAddress 192.168.137.1 -LocalPort 30490 -RemoteAddress 192.168.137.69
New-NetFirewallHyperVRule -Name 'SOMEIP-SD-WSL' -DisplayName 'SOMEIP SD Pi to WSL' -Direction Inbound -Action Allow -VMCreatorId '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}' -Protocol UDP -LocalPorts 30490 -RemoteAddresses 192.168.137.69
~~~

기존 이름의 규칙이 있으면 중복 생성하지 말고 주소·포트·활성 상태를 확인한다.

## 3. Pi 서버

Pi에서 `vcan0`가 올라왔는지 확인한 다음 터미널 하나에서 실행한다.
Pi의 현재 미커밋 런처에서는 근무지 프로필 이름이 `work`이고
`provider_pi.json`에 근무지 주소가 들어 있다. PC 저장소의 분리된 버전은
`work`가 `provider_office.json`을 읽는다.

~~~sh
ip link show vcan0
./scripts/run_vehicle_server.sh --someip-profile work --scenario acceleration --interface vcan0 --interval 1.0
~~~

서버는 한 개만 실행한다. 종료는 서버 터미널에서 Ctrl+C.

## 4. PC/WSL 클라이언트

다른 PC/WSL 터미널에서 순서대로 실행한다.

~~~sh
./scripts/run_vehicle_client.sh --someip-profile work method
./scripts/run_vehicle_client.sh --someip-profile work subscribe 3
./scripts/run_vehicle_client.sh --someip-profile work body
./scripts/run_vehicle_client.sh --someip-profile work body-subscribe 3
./scripts/run_vehicle_client.sh --someip-profile work door 0 open
./scripts/run_vehicle_client.sh --someip-profile work door 0 close
~~~

문 인덱스는 `0` 운전석, `1` 조수석, `2` 뒷좌석 왼쪽, `3` 뒷좌석 오른쪽,
`4` 트렁크다. `Door command accepted`는 CAN 송신 접수만 뜻한다.
`BodyStatus flags=0x1 driver_door=open` 또는
`BodyStatus flags=0x0 driver_door=closed`를 보고 실제 상태를 확인한다.
상태 비트·페이로드 계약은 [body_control_protocol.md](body_control_protocol.md)에 있다.

## 5. GUI

~~~sh
./scripts/run_vehicle_dashboard.sh --someip-profile work
~~~

Windows 또는 WSL 브라우저에서 `http://127.0.0.1:8765`를 연다.
속도·RPM·냉각수와 문 네 개·트렁크 상태를 표시한다. 운전석 문 버튼에서
**명령 접수**와 **BodyStatus로 확인한 실제 상태**를 별도로 표시한다.
연결이 끊기거나 상태가 오래되면 값을 `확인 중`으로 바꾸고 버튼을 비활성화한다.
포트가 사용 중이면 `--port 8766`처럼 지정한다. GUI는 SOME/IP에서 해석한
차량·본문 데이터만 사용하고 raw CAN에 접근하지 않는다.

## 6. 패킷 확인

Pi와 PC/WSL 각각에서 별도 터미널로 실행한다. `sudo` 비밀번호가 필요하다.

~~~sh
sudo timeout 20 tcpdump -ni eth0 -vv 'udp port 30490 or tcp port 30540'
~~~

Pi의 `192.168.137.69:30490 → 224.244.224.245:30490`은 SD 송신이다.
PC에서도 같은 송신을 수신하고 클라이언트 로그의 `ON_OFFER_SERVICE`가
나와야 서비스 발견이 확인된다. `192.168.137.1:<임시 포트> →
192.168.137.69:30540` TCP SYN/SYN-ACK와 페이로드 왕복은 메서드·이벤트
전송 경로다. UDP 포트 표시만으로 특정 SD 메시지 종류를 단정하지 않는다.

집에서는 서버 `--someip-profile pi`, PC 클라이언트와 GUI
`--someip-profile pc`를 사용하며, 먼저 집 주소와 방화벽 규칙을 다시 확인한다.
