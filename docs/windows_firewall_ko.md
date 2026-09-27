# SOME/IP SD 방화벽 주소 변경

## Windows 방화벽: 기존 규칙 제거 후 새 IP 적용

집·근무지 모두 Pi `192.168.137.2/24`, PC/WSL `192.168.137.1/24`를 사용한다.
아래 명령은 **Windows 관리자 PowerShell**에서 실행한다. 저장소 수정만으로 Windows 규칙이 바뀌지는 않는다.
전체 방화벽 초기화 대신 이전에 만든 두 SOME/IP 규칙만 제거하고 다시 만든다.
먼저 주소 필터를 확인한다. 이전 Pi 주소는 `192.168.50.2` 또는 `192.168.137.69`일 수 있다.

~~~powershell
Get-NetFirewallRule -DisplayName 'SOMEIP SD Pi to PC' -ErrorAction SilentlyContinue | Get-NetFirewallAddressFilter
Get-NetFirewallHyperVRule -Name 'SOMEIP-SD-WSL' -ErrorAction SilentlyContinue | Format-List *

Get-NetFirewallRule -DisplayName 'SOMEIP SD Pi to PC' -ErrorAction SilentlyContinue | Remove-NetFirewallRule
Get-NetFirewallHyperVRule -Name 'SOMEIP-SD-WSL' -ErrorAction SilentlyContinue | Remove-NetFirewallHyperVRule

New-NetFirewallRule -DisplayName 'SOMEIP SD Pi to PC' -Direction Inbound -Action Allow -Profile Any -Protocol UDP -LocalAddress 192.168.137.1,224.244.224.245 -LocalPort 30490 -RemoteAddress 192.168.137.2
New-NetFirewallHyperVRule -Name 'SOMEIP-SD-WSL' -DisplayName 'SOMEIP SD Pi to WSL' -Direction Inbound -Action Allow -VMCreatorId '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}' -Protocol UDP -LocalPorts 30490 -RemoteAddresses 192.168.137.2

Get-NetFirewallRule -DisplayName 'SOMEIP SD Pi to PC' | Get-NetFirewallAddressFilter
Get-NetFirewallRule -DisplayName 'SOMEIP SD Pi to PC' | Get-NetFirewallPortFilter
Get-NetFirewallHyperVRule -Name 'SOMEIP-SD-WSL' | Format-List *
~~~

Windows 규칙은 SD 유니캐스트와 멀티캐스트 목적지 둘 다 허용한다. `Profile Any`는
집·근무지의 Public/Private 분류가 바뀌어도 적용하기 위한 것이며, 송신자는 Pi 한 대,
프로토콜·포트는 UDP 30490으로 제한한다. Hyper-V 규칙은 WSL VM과 Pi 송신자에 제한한다.
다른 이름으로 만든 규칙은 이 절차가 제거하지 않는다. Hyper-V cmdlet이 없으면 그 단계를
건너뛰어 성공으로 판단하지 말고 Windows/WSL 버전과 mirrored 모드를 확인한다.

이 규칙을 나중에 제거하려면 위의 두 `Get-... | Remove-...` 명령만 실행한다.
방화벽 적용 후 양쪽 SD 경로와 클라이언트 메서드·이벤트 수신을 다시 확인한다.
