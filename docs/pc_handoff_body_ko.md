# PC/WSL Codex 전달 문안

아래 내용을 PC/WSL Codex 채팅에 붙여 넣는다.

~~~text
SOMEIP-CAN_Project의 PC/WSL 측 GUI와 멀티호스트 SOME/IP 통신을 이어서 구현해줘.
현재 세션은 Raspberry Pi 5에서 작업했고, Pi 저장소의 변경은 아직 커밋/푸시하지 않았다.
PC 저장소와 자동 동기화됐다고 가정하지 말고, 먼저 양쪽 git status와 파일을 비교해줘.
Pi에 SSH 접근이 가능하면 변경 파일을 안전하게 가져오되 PC의 미커밋 수정은 덮어쓰지 마.
접근이 안 되면 필요한 전송 방법과 파일 목록을 구체적으로 알려줘.

Pi에서 구현·검증한 내용:
- config/can/body.json: 상태 CAN ID 0x200(DLC 1), 문 명령 ID 0x201(DLC 2).
- 상태 비트 0=운전석 문, 1=조수석 문, 2=뒷좌석 왼쪽 문,
  3=뒷좌석 오른쪽 문, 4=트렁크, 5=문 잠금, 6=전조등. 비트 0~4는 1=열림.
- 명령 페이로드: byte0=문/트렁크 인덱스 0~4, byte1=0 닫기/1 열기.
- 기존 SOME/IP service 0x6301/instance 0x0001/TCP 30540에
  GetBodyStatus method 0x0004(응답 1바이트 상태),
  SetDoor method 0x0005(요청 2바이트; 응답 1은 CAN 송신 접수),
  BodyStatus event 0x8002/eventgroup 0x0002(1바이트 상태)가 추가됐다.
- SetDoor의 응답만으로 문 상태 변경을 확정하지 말 것. BodyStatus 이벤트 또는
  GetBodyStatus의 목표 비트 변경으로 확인할 것.
- 기존 VehicleData event 0x8001(8바이트), 속도/RPM/냉각수 method는 유지된다.
- Pi의 가상 ECU는 vcan0에서 명령을 받아 상태 CAN 프레임을 다시 송신한다.
  실제 차량 문이나 물리 CAN HAT을 제어하지 않는다.
- Pi에서 candump로 201#0001 -> 200#01, 201#0000 -> 200#00을 확인했고,
  로컬 SOME/IP 클라이언트가 driver_door=open/closed를 받았다.
  C++ 테스트 4개, Python·통합 테스트 12개 통과. PC Ethernet/GUI는 미검증.
- 상세 계약: docs/body_control_protocol.md. Pi 실행 절차: docs/runbook.md.

PC/WSL에서 할 일:
1. 먼저 Pi 변경 파일을 PC 저장소에 반영하고 빌드·테스트해줘.
   새로운 vehicle_body_client와 run_vehicle_client.sh의 body,
   body-subscribe COUNT, door INDEX open|close 모드가 있어야 한다.
2. PC/WSL 실제 IP와 라우트를 확인해줘. 근무지 Pi는 현재 eth0
   192.168.137.2/24이며 Pi에서는 --someip-profile work를 사용한다.
   기존 client_pc.json의 192.168.50.1은 집 주소이므로 근무지에 그대로
   적용하지 마. WSL 자체가 가진 IP, Pi 192.168.137.2 경로,
   224.244.224.245 multicast 경로, Windows/WSL 네트워크 모드를 실측해
   PC 프로필을 맞춰줘. 집 설정은 보존하고 근무지 설정은 분리해줘.
3. Pi 서버 한 개만 실행한 상태에서 PC에서 method, subscribe 3, body,
   body-subscribe 3, door 0 open, door 0 close를 실행하고 양쪽 tcpdump로
   SD 수신, TCP 연결, 메서드 응답, 바디 이벤트를 구분해 검증해줘.
4. GUI에는 속도·RPM·냉각수와 다섯 개 문/트렁크 상태를 표시하고,
   가상 운전석 문 열기/닫기 버튼을 연결해줘. 명령 접수와 실제 상태 변경을
   별도로 표시하고 timeout/연결 끊김을 처리해줘. GUI가 raw CAN을 직접
   읽거나 쓰지 않게 해줘. 잠금/전조등은 현재 상태 비트만 정의됐고 명령은
   구현되지 않았으므로 버튼을 동작하는 것처럼 만들지 마.
5. PC 저장소의 기존 GUI 구조를 먼저 확인하고 그 구조에 최소한으로 연동해줘.
   필요한 자동화 테스트와 수동 확인 절차를 추가하고 docs/runbook.md를
   실제 PC 절차에 맞게 갱신해줘. PC에서 통신과 GUI 동작을 실제로 보기
   전에는 멀티호스트 완성 상태를 표시하지 마.

마지막에 변경 파일, 테스트 결과, PC/WSL의 실제 IP·라우트, Pi OfferService
수신 여부, TCP 30540 연결 여부, 명령과 상태 이벤트 결과, 남은 미검증 사항을
간결하게 보고해줘.
~~~
