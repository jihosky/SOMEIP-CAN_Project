# SOMEIP-CAN 프로젝트 요약

## 1. 프로젝트 개요

**이름:** SOMEIP-CAN Project. CMake의 프로젝트 이름은 `SOMEIP_CAN_Project`다.

**목표:** 가상 차량 데이터를 Linux CAN 인터페이스로 보내고 C++ 게이트웨이에서 해석한 뒤 SOME/IP 서비스로 제공하는 차량 소프트웨어 플랫폼을 단계적으로 구축한다. 현재 구현 범위는 가상 ECU부터 SOME/IP 클라이언트까지다.

프로젝트를 시작할 때 Raspberry Pi 5와 이중 채널 CAN HAT은 있었지만, 신뢰성 있게 물리 CAN 버스를 시험할 독립 ECU 또는 CAN 노드가 없었다. 그래서 Windows + WSL2에서 vCAN 기반의 재현 가능한 통신 경로를 먼저 만들었다. 최종 방향은 고수준 차량 API, PC 측 AI 진단 에이전트, 진단 화면과 보고서, 물리 CAN 및 Raspberry Pi 검증이다. Pi의 CAN 수신·디코딩과 SOME/IP 송신에 이어 PC/WSL의 서비스 발견, 원격 메서드 응답 및 이벤트 3개 수신까지 직접 연결에서 검증했다.

## 2. 전체 아키텍처

현재 구현된 데이터 흐름은 다음과 같다.

```text
Python Virtual ECU (python-can)
          |
          v
      vcan0 (vCAN)
          |
          v
SocketCAN 수신기 -> CanFrame -> SignalDecoder -> VehicleData
                                                |
                                                v
                                         VehicleService
                                                |
                                                v
                                   VehicleDataProvider (vSomeIP)
                                                |
                                메서드 응답 / VehicleData 이벤트
                                                |
                                                v
                                       VehicleDataClient
```

`vehicle_gateway`는 `SocketCanReceiver`, `SignalDecoder`, `VehicleService`, `VehicleDataProvider`를 한 프로세스에서 구성한다. CAN 수신 스레드는 새 샘플을 디코딩해 서비스에 저장하고 제공자에 전달한다. 제공자는 최신 서비스 값을 메서드 응답으로 읽으며 새 샘플마다 이벤트를 발행한다. `VehicleService`는 뮤텍스로 최신 데이터 접근을 보호한다. AI 계층과 대시보드는 아직 연결되지 않았다.

## 3. 개발 환경

주 개발 환경은 **Windows + WSL2**의 Linux다. VS Code로 편집하고 Git으로 변경 사항을 관리하며, 개발 작업에 Codex를 사용한다. C++17, CMake와 Ninja로 SocketCAN 게이트웨이 및 vSomeIP 제공자·클라이언트를 빌드한다. Python 3.10 이상과 `python-can`은 가상 ECU에, `pytest`는 Python 및 통합 테스트에 사용한다. `can-utils`의 `candump`는 수동 프레임 관찰용이다. vCAN은 가상 CAN 인터페이스이고 SocketCAN은 Linux CAN 소켓 API다. 설치·준비 명령은 [실행 지침](runbook.md)에 있다.

## 4. 프로젝트 구조

| 경로 | 책임 |
| --- | --- |
| `config/` | `can/milestone1.json`의 임시 CAN 정의와 `someip/`의 제공자·클라이언트 설정 |
| `cpp/` | `can_gateway/`의 수신·디코딩, `vehicle_service/`의 차량 모델·서비스, `someip/common/`의 식별자·코덱, `someip/provider/`와 `someip/client/`, `apps/`의 통합 실행 파일 |
| `python/` | `virtual_ecu/`의 결정적 차량 샘플 생성 및 CAN 전송 |
| `tests/` | `cpp/` 단위, `python/` 단위, `integration/`의 vCAN·SOME/IP·런처 테스트 |
| `docs/` | 아키텍처, 인터페이스, 개발·테스트·실행 지침과 `decisions/`의 설계 결정 기록 |
| `scripts/` | 두 터미널 실행 스크립트와 한 명령 시연 스크립트 |

## 5. 마일스톤 진행 내용

| 단계 | 구현 내용 |
| --- | --- |
| **1A** | Python 가상 ECU가 `python-can` SocketCAN 백엔드로 `vcan0`에 전송한다. C++ 수신기가 원시 CAN ID, DLC, 데이터와 수신 시각을 출력한다. |
| **1B** | Linux 자료형에 의존하지 않는 `CanFrame`, 별도 `SignalDecoder`, 임시 `0x100` 정의, `VehicleData` 및 읽기 전용 `VehicleService`를 추가했다. |
| **2A** | vSomeIP 기반 `VehicleDataProvider`와 클라이언트를 분리하고 `GetVehicleSpeed`, `GetEngineRpm`, `GetCoolantTemperature` 요청·응답 및 이진 코덱을 구현했다. 로컬 시험에는 미리 넣은 서비스 값을 사용한다. |
| **2B** | `vehicle_gateway`가 실시간 CAN 수신, 디코딩, 공유 `VehicleService`, SOME/IP 제공자를 소유한다. 실시간 경로에는 사전 설정한 차량 값을 넣지 않는다. |
| **2C** | 가상 ECU에 `steady`와 `acceleration` 시나리오를 추가했다. 제공자는 새 샘플마다 이벤트를 발행하고, 클라이언트는 서비스 발견 후 이벤트 그룹에 구독한다. 기존 메서드 API도 유지된다. |
| **개발 도구** | `run_vehicle_server.sh`가 ECU와 게이트웨이를 시작·정리하고, `run_vehicle_client.sh`가 메서드/구독 모드를 실행한다. `run_demo.sh`는 고정 값으로 빠른 시연을 수행한다. [실행 지침](runbook.md)은 두 터미널 및 상세 디버깅 절차를 제공한다. |

## 6. CAN 데이터 정의

현재 정의는 [`config/can/milestone1.json`](../config/can/milestone1.json)의 **표준 CAN ID `0x100`, DLC 8**이다. 여러 바이트 값은 **리틀 엔디언**이다.

| 바이트 | 신호 | 원시 형식 | 물리 값 |
| --- | --- | --- | --- |
| 0–1 | 차량 속도 | 부호 없는 16비트 | 원시 값 × 0.01 km/h |
| 2–3 | 엔진 회전수 | 부호 없는 16비트 | 원시 값 × 1 rpm |
| 4 | 냉각수 온도 | 부호 없는 8비트 | 원시 값 − 40 °C |
| 5–7 | 예약 | 가상 ECU가 0으로 채움 | 디코더가 무시 |

`steady` 샘플은 123.45 km/h, 2500 rpm, 85 °C이며 데이터는 `39 30 C4 09 7D 00 00 00`이다. `acceleration`은 20/1200/70 → 40/1800/71 → 60/2400/72를 반복한다. 이 정의는 임시 계약이며 장기적으로 DBC 기반 정의로 발전시킬 계획이다.

## 7. SOME/IP 설계

SOME/IP는 해석된 차량 정보를 서비스의 메서드와 이벤트로 전달하기 위해 선택했다. 현재 설치된 vSomeIP를 C++ 제공자와 클라이언트에 사용한다. 식별자의 단일 출처는 [`identifiers.hpp`](../cpp/someip/common/include/vehicle_someip/identifiers.hpp)다.

| 항목 | 식별자 | 동작 |
| --- | --- | --- |
| `VehicleDataService` | 서비스 `0x6301`, 인스턴스 `0x0001` | 차량 정보 서비스 |
| `GetVehicleSpeed` | 메서드 `0x0001` | 최신 속도 요청 |
| `GetEngineRpm` | 메서드 `0x0002` | 최신 회전수 요청 |
| `GetCoolantTemperature` | 메서드 `0x0003` | 최신 냉각수 온도 요청 |
| `VehicleData` | 이벤트 `0x8001`, 이벤트 그룹 `0x0001` | 새 샘플 알림 |

메서드 요청 본문은 비어 있다. 응답은 속도 × 100을 부호 없는 32비트, RPM을 부호 없는 16비트, 온도를 부호 있는 16비트로 인코딩한다. 이벤트는 세 값을 이 순서로 묶은 8바이트다. **SOME/IP 페이로드는 빅 엔디언**이며 CAN 페이로드 표현과 별개다. 클라이언트는 vSomeIP Service Discovery가 서비스 가용성을 알린 후 이벤트를 요청하고 그룹에 구독한다. 제공자는 `VehicleService`만 참조하며 CAN ID와 원시 프레임 형식을 알지 않는다. 자세한 계약은 [인터페이스 문서](interfaces.md)에 있다.

## 8. 소프트웨어 계층 분리

```text
SocketCAN -> CAN 도메인(CanFrame·SignalDecoder) -> VehicleService
                                              -> SOME/IP 제공자·클라이언트
                                              -> 향후 고수준 Vehicle API -> AI
```

SocketCAN은 Linux 소켓과 프레임 수신을 담당하고 디코더는 CAN 정의를 차량 값으로 변환한다. `VehicleData`와 `VehicleService`에는 SocketCAN 또는 SOME/IP 자료형이 없다. SOME/IP 계층은 도메인 값을 직렬화한다. 미래 AI 에이전트는 고수준 차량 API만 사용하며 CAN ID, 원시 페이로드, SOME/IP 메시지를 직접 다루지 않는다. 이 API와 AI 계층은 **계획 단계**다.

## 9. 테스트 및 검증 현황

현재 저장소에서 수집되는 테스트는 **CTest 4개, Python 단위 4개, `integration` 표시 통합 7개**다. 통합 7개에는 런처 관련 3개가 포함된다. 이전 런처 작업에서 CMake 구성·빌드, CTest 4개, 당시 Python 단위 2개, 통합 7개, `run_demo.sh`, `git diff --check`가 통과했다. 아래 범위는 실제 테스트 파일과 수집 결과를 기준으로 한다.

| 구분 | 검증 내용 |
| --- | --- |
| C++ 단위 | 프레임 출력, 디코딩 경계 값, 서비스 최신 값·동시 접근, SOME/IP 메서드·이벤트 코덱 |
| Python 단위 | 고정 CAN 프레임과 가속 시나리오의 세 페이로드, 로컬/멀티 호스트 SOME/IP 설정 계약 |
| vCAN 통합 | 가상 ECU → C++ 수신기 → 원시·디코딩 출력 |
| SOME/IP 메서드 통합 | 고정 서비스 값 → 제공자 → 클라이언트 및 실시간 ECU → 게이트웨이 → 클라이언트 |
| SOME/IP 이벤트 통합 | 가속 시나리오의 세 샘플 순서와 값 |
| 런처 통합 | 잘못된 인수·실행 파일 누락, 구독 경로·자식 종료, 한 명령 시연 |

통합 테스트는 빌드된 실행 파일과 필요한 vCAN 인터페이스를 사용한다. 일부 테스트는 인터페이스 또는 실행 파일이 없으면 건너뛰므로 **수집 수와 환경별 실제 실행 수는 구별해야 한다**. 명령은 [테스트 문서](testing.md)와 [실행 지침](runbook.md)에 있다.

## 10. 실행 방법 요약

[실행 지침](runbook.md)에 따라 Python 환경, vSomeIP, 빌드와 `vcan0`를 준비한 뒤 저장소 루트에서 실행한다.

**터미널 1 — 서버:**

```sh
./scripts/run_vehicle_server.sh --scenario acceleration
```

**터미널 2 — 이벤트 클라이언트:**

```sh
./scripts/run_vehicle_client.sh subscribe 3
```

메서드를 확인하려면 `./scripts/run_vehicle_client.sh method`를 실행한다. 고정 값까지 한 번에 확인하려면 서버를 별도로 실행하지 않은 상태에서 `./scripts/run_demo.sh`를 실행한다. 서버 스크립트의 Ctrl+C는 자신이 시작한 ECU와 게이트웨이를 종료한다.

## 11. 주요 설계 결정

| 결정 | 요약 |
| --- | --- |
| [ADR 0001: vCAN 우선](decisions/0001-use-vcan-first.md) | 독립 물리 CAN 노드 없이 재현 가능한 개발·통합 테스트를 먼저 수행한다. |
| [ADR 0002: 계층 경계](decisions/0002-layer-boundaries.md) | CAN 디코딩, 차량 서비스, SOME/IP, 향후 AI API의 책임을 분리한다. |
| [ADR 0003: vSomeIP](decisions/0003-use-vsomeip.md) | 설치된 vSomeIP를 로컬 서비스 메서드에 사용하고 식별자·코덱·제공자·클라이언트를 분리한다. 이후 단계에서 이벤트와 Service Discovery가 추가되었다. |

## 멀티 호스트 SOME/IP 조사 상태

- **구현:** Pi용 `config/someip/provider_pi.json`과 PC/WSL용 `client_pc.json`을 로컬 루프백 설정과 분리했다. 런처의 `--someip-profile pi|pc`는 설정 경로와 애플리케이션 이름을 출력하고 주소·SD 멀티캐스트 경로를 검사한다. 제공자는 등록과 `offer_service()` 호출 시점을 기록한다.
- **로컬 검증:** 기존 vCAN → 게이트웨이 → SOME/IP 메서드·이벤트 테스트는 한 호스트에서 수행한다. 로컬 성공은 Ethernet 전달의 증거가 아니다. 현재 작업 환경에는 `vcan0`가 없어 이번 조사에서 vCAN 의존 통합 테스트는 건너뛰었다.
- **Pi 측 현장 확인:** 소유자가 CAN 수신·디코딩, `192.168.50.2:30540` LISTEN 및 `eth0`의 UDP 30490 SD 송신을 확인했다. Pi의 원시 캡처는 이 작업 환경에서 직접 수행하지 않았다.
- **멀티 호스트 실측 완료:** mirrored WSL의 `eth0`와 Windows Ethernet은 `192.168.50.1/24`였다. Windows에서 Pi `192.168.50.2:30490`의 SD UDP 패킷 56바이트를 수신했다. WSL `vehicle-client`는 `client_pc.json`을 읽고 `0x6301/0x0001` 서비스를 발견했다. `192.168.50.1:<임시 포트> → 192.168.50.2:30540` TCP 연결이 `ESTAB`였으며, 세 메서드(60.00 km/h, 2400 rpm, 72 C)와 가속 이벤트 세 개(60/2400/72 → 20/1200/70 → 40/1800/71)를 수신했다.
- **원인 및 제한:** Windows Public과 WSL Hyper-V의 인바운드 기본 정책은 `Block`이었다. Pi 주소·UDP 30490에 한정한 허용 규칙 적용 전에는 SD 수신과 메서드가 실패했고, 적용 후 성공했다. WSL의 멀티캐스트 경로가 재설정 과정에서 `eth0`에서 `eth1`로 바뀐 적도 있다. WSL `tcpdump`는 권한 부족으로 실행하지 못했다.
- **아직 미검증:** 장시간 연결 안정성, 링크 재연결 뒤 복구, 패킷 손실, 다른 주소·네트워크·호스트에서의 서비스 발견은 [실행 지침](runbook.md)에 따라 추가 검증이 필요하다.

## 12. 현재까지 확인된 한계

- **미검증:** 물리 CAN 버스·배선·종단·CAN HAT과 `can0`/`can1` 경로, Raspberry Pi의 지속적 배포·재시작 운영, 다른 네트워크의 SOME/IP Service Discovery, 패킷 손실과 복구 동작.
- **미구현:** AI Agent, 고수준 Python Vehicle API, UDS 진단, 대시보드, 진단 보고서.
- **현재 범위:** `0x100` 신호 정의와 이진 서비스 페이로드는 시연용 계약이다. 외부 클라이언트에 공개하기 전 DBC 기반 정의와 버전 관리가 필요하다.

## 13. 다음 개발 단계

| 단계 | 계획이며 현재 미구현인 내용 |
| --- | --- |
| **3A** | Python 고수준 Vehicle API로 SOME/IP 통신을 차량 조회 연산 뒤에 숨김 |
| **3B** | AI Agent와 도구 호출을 연결해 차량 상태를 고수준 API로 조회 |
| **3C** | 고장 주입과 AI 기반 진단 흐름 개발 |
| **이후** | 대시보드, 진단 보고서, 물리 CAN 검증, Raspberry Pi 배포, 포트폴리오 시연 |

## 14. 포트폴리오 관점 핵심 포인트

현재 결과물은 Linux vCAN/SocketCAN 통신, Python 송신기와 C++ 게이트웨이의 혼합 구조, 차량 서비스 경계, vSomeIP 요청·응답과 Service Discovery 기반 이벤트 구독을 보여준다. CMake·CTest·pytest 자동 검증, 재현 가능한 런처와 실행 지침, Codex를 활용한 단계별 개발 기록도 검토할 수 있다. 물리 차량 통신과 AI 진단 성능을 입증하는 자료는 아직 아니다.

## 15. 현재 상태 한 줄 요약

현재 vCAN 기반 동적 차량 데이터를 C++ 게이트웨이에서 처리하여 SOME/IP 메서드와 이벤트로 전달하는 종단 간 차량 소프트웨어 경로를 구현하고 로컬 환경에서 검증했다.
