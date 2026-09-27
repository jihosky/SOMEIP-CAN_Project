# Pi VehicleState integration validation — 2026-09-27

Status: integration and finite Ethernet event streaming verified; full milestone pending.

## Branches and changes

1. Starting branch: clean `feat/pi-gateway`, `8d00e0d`, matching origin.
2. PC source: `origin/feat/vehicle-state-streaming`, `56ec470`.
   `origin/feat/body-control-dashboard` had the same tip; main was `fa4cf15`.
3. Pi source: `origin/feat/pi-gateway`, `8d00e0d`.
4. Strategy: branch from PC streaming, selectively port Pi shutdown/preflight code.
   Keep current PC network profiles and source implementation. Preserve historical docs.
5. Result: `feat/pi-vehicle-state-streaming`. Original branches unchanged; no force push.
6. No merge/cherry-pick was run, so no textual conflicts. Semantic overlaps in
   profiles, launchers and docs were reviewed rather than choosing ours/theirs.
7. Files changed relative to PC tip:
   - `scripts/run_vehicle_server.sh`: HUP cleanup, shutdown logs, duplicate gateway guard.
   - `scripts/stop_vehicle_server.sh`: restored executable Pi recovery command.
   - `tests/integration/test_launchers.py`: HUP child cleanup regression.
   - `docs/runbook.md`: current integration commands/status.
   - This validation report.
   - `docs/history/pi-runtime-2026-09-27/{runbook.md,commands_ko.md,pc_handoff_body_ko.md,body_control_protocol.md}`:
     archived Pi documents, marked historical.
   No GUI, protocol, codec or network JSON edits; Pi address already correct.

## Build and runtime

8. `cmake -S . -B build -G Ninja` and `cmake --build build`: passed.
9. `ctest --test-dir build --output-on-failure`: 4 passed, zero failed/skipped.
   `PYTHONPATH=python .venv/bin/python -m pytest tests/python tests/integration -ra -q`:
   22 passed in 7.33 s, zero failed/skipped. Includes repeated identical samples,
   malformed payloads, delayed provider/service loss and launcher shutdown.
10. eth0 UP, 192.168.137.2/24; vcan0 UP. PC .1 reachable (2/2 ping).
    Route to PC .1 and 224.244.224.245 uses eth0, source .2. No old provider at startup.
11. Actual process environment and logs loaded `config/someip/provider_pi.json`,
    application `vehicle-provider`. Started using:
    `./scripts/run_vehicle_server.sh --someip-profile pi --scenario acceleration --interface vcan0 --interval 1.0`.
12. Provider reached ST_REGISTERED. CAN decode/service log cycles
    20/1200/70 → 40/1800/71 → 60/2400/72.
13. Actual eth0 SD OfferService: service 0x6301, instance 0x0001, TTL 3;
    endpoint 192.168.137.2, TCP 30540.
14. PC .1 SubscribeEventgroup 0x0001 and Pi positive-TTL Ack captured.
15. PC finite subscription received three 0x8001 NOTIFICATIONs (type 0x02).
16. TCP 192.168.137.2:30540 listener owned by gateway confirmed.
17. UDP 30490 sockets and outgoing/incoming SD confirmed.
18. PC methods returned 20 km/h, 1200 rpm, 70 C. Pi-local method after event
    subscription also passed. PC method after continuous subscription is pending.
19. Pi-local continuous subscriber received six events, remained alive, then
    SIGINT exited 0 without a remaining client. This is local UDS evidence,
    not a substitute for PC continuous Ctrl+C/process verification (pending).

## Ethernet evidence

20. Capture: `/tmp/pi-vehicle-state-20260927/pc-validation.pcap` (eth0).
    Temporary evidence also includes `provider.log`, `can.log`, `decoded.json`,
    `decode_capture.py`, local finite/continuous/method logs. /tmp is not durable.
    Capture parsed offline with IPv4/TCP stream reassembly; observed streams had
    no sequence gaps or trailing incomplete SOME/IP messages. PC port 45267
    received the following event payloads, matching the user's PC log:

| Payload hex (8 bytes) | Speed km/h | RPM | Coolant °C |
|---|---:|---:|---:|
| 00000fa007080047 | 40 | 1800 | 71 |
| 0000177009600048 | 60 | 2400 | 72 |
| 000007d004b00046 | 20 | 1200 | 70 |

    Wire layout: uint32 speed*100, uint16 RPM, int16 coolant, all big-endian.
    No raw CAN ID/DLC. CAN 0x100 examples were D007B0046E000000,
    A00F08076F000000, 7017600970000000. Codec tests reject null/7/9-byte inputs.
    Finite event stream contains no repeated getter requests. Source publishes
    after each decoded CAN sample/service update, force=true, without an event timer.
    Subscription renewal is SD maintenance, not method polling.

## Remaining work and limitations

21. Pending: PC continuous 10+ seconds, Ctrl+C exit status and absence of stale
    client; PC method regression after event streaming; separate PC Wireshark confirmation.
    User's PC method log has `Invalid client id` despite successful values.
    Local vSomeIP source `routing_manager_impl::is_valid_client_id` checks response
    client-ID diagnosis prefix; client_pc.json has ID 0x6302 and no explicit diagnosis.
    This check logs but does not discard the response in the inspected source.
    Confirm PC library/config diagnosis settings before changing client IDs or config.
    PC also reports device-bind and requested-reliability fallback warnings; actual
    captured transport is TCP. These warnings are not recorded as error-free success.
22. Full milestone cannot yet be marked complete. GUI unchanged: events → bridge
    → cached Python state → dashboard. No multicast event or multi-subscriber
    validation claimed. `project_summary_ko.md` remains unchanged pending completion.
