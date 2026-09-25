# 근무지 Pi–PC 검증 기록 (2026-09-26)

## 저장소와 반영 범위

- PC HEAD `fa4cf15`, Pi HEAD `274d7ed`. Pi는 이전 커밋 위에 본문 제어 변경이 미커밋 상태였고, PC에는 GUI와 근무지 주소 변경이 미커밋 상태였다.
- Pi의 `git status --short`와 `docs/body_control_protocol.md`를 확인하고 Pi 파일을 `/tmp/someip-pi-snapshot`에 별도 복사해 비교했다. Pi 저장소는 수정·커밋·푸시하지 않았다.
- Pi의 본문 CAN/서비스/SOME/IP 계약과 테스트를 PC에 가져왔다. PC의 후속 멀티호스트 로그·설정 변경과 겹치는 게이트웨이·제공자는 공통 조상 기준으로 병합했다. PC GUI와 기존 미커밋 파일을 덮어쓰지 않았다.
- 집의 `client_pc.json=192.168.50.1`, `provider_pi.json=192.168.50.2`를 유지했다. 근무지 `client_office.json=192.168.137.1`, `provider_office.json=192.168.137.69`를 별도로 만들었다. 각 집·근무지 JSON은 `unicast`만 다르다는 테스트가 통과했다.

## 실측 네트워크

| 항목 | 확인 결과 |
| --- | --- |
| PC/WSL `eth0` | `192.168.137.1/24` |
| PC/WSL `eth1` | `10.183.1.77/24` |
| Pi `eth0` | `192.168.137.69/24` |
| PC→Pi | `192.168.137.69 dev eth0 src 192.168.137.1` |
| PC SD 멀티캐스트 | `224.244.224.245 dev eth0 src 192.168.137.1` |
| Pi SD 멀티캐스트 | `224.244.224.245 dev eth0 src 192.168.137.69` |
| PC TCP | `192.168.137.1:<임시 포트> → 192.168.137.69:30540 ESTAB` |
| PC UDP | `192.168.137.1%eth0:30490` 바인딩 |

## PC에서 Pi 서버로 실행한 결과

| 명령 (`--someip-profile work`) | 실측 결과 |
| --- | --- |
| `method` | 속도 20.00 km/h, RPM 1200, 냉각수 70 °C 응답; `ON_OFFER_SERVICE`, `ON_AVAILABLE` |
| `subscribe 3` | 40/1800/71, 60/2400/72, 20/1200/70 이벤트 3개 |
| `body` | `flags=0x0`, 다섯 문·트렁크 닫힘 |
| `body-subscribe 3` | `flags=0x0` 이벤트 3개 |
| `door 0 open` | `Door command accepted` 뒤 `flags=0x1 driver_door=open` |
| `door 0 close` | `Door command accepted` 뒤 `flags=0x0 driver_door=closed` |

PC GUI의 단일 SOME/IP 연결에서도 차량·본문 이벤트를 동시에 수신했다. HTTP 버튼 API로 열기·닫기를 각각 실행했을 때 `Accepted for CAN transmission`과 `Confirmed open/closed by BodyStatus`가 별도로 표시됐다. 브라우저 화면의 차량 값·문 상태·버튼 표시도 사용자가 확인했다. Pi 서버가 종료됐을 때 GUI가 `Disconnected; reconnecting`으로 바뀌고 상태를 오래된 값으로 표시하지 않는 것도 관찰했다. Timeout 상태 전이는 Python 테스트로 확인했다.

## 패킷과 검증 범위

Pi의 `/tmp/someip-pi-verify.pcap`에서 75개 패킷을 캡처했다. `192.168.137.69:30490 → 224.244.224.245:30490` SD 패킷, PC의 UDP 30490 메시지, `192.168.137.1:<임시 포트> → 192.168.137.69:30540` TCP SYN/SYN-ACK 및 양방향 페이로드를 확인했다. PC 클라이언트의 `ON_OFFER_SERVICE`와 `ON_AVAILABLE` 로그는 Pi OfferService 수신 증거다. 메서드 응답과 바디 이벤트의 내용은 클라이언트 로그와 GUI API에서 구분해 확인했다.

**남은 검증:** PC/WSL의 raw `tcpdump` 캡처는 `sudo` 비밀번호가 필요한 환경이라 아직 확보하지 못했다. Windows UDP 수신 소켓 단독 시험도 5초 동안 패킷을 받지 못했으며, WSL 클라이언트의 실제 수신 결과를 뒤집는 증거로 해석하지 않는다. PC 캡처가 필요하면 [실행 명령](commands_ko.md)의 `sudo timeout 20 tcpdump ...`를 사용자 터미널에서 실행해 Pi 캡처와 비교한다. Pi 테스트용 서버가 몇 분 뒤 `[SETUP] A child exited unexpectedly`로 종료된 사례가 있어 원인과 장시간 안정성은 미확인이다. 집 네트워크 재시험, 물리 CAN 및 인증 보안도 이번 검증 범위 밖이다.

## 자동 검사

- `cmake -S . -B build -G Ninja` 및 `cmake --build build`: 성공
- `ctest --test-dir build --output-on-failure`: 4/4 통과
- `PYTHONPATH=python ./.venv/bin/python -m pytest tests/python -q`: 9/9 통과
- `PYTHONPATH=python ./.venv/bin/python -m pytest tests/integration -m integration -q`: 8/8 통과
- `git diff --check`: 오류 없음
