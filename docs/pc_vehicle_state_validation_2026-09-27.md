# PC VehicleState follow-up validation — 2026-09-27

## Repository preservation and source

Fetched `origin/feat/pi-vehicle-state-streaming`, tip
`98f203c6278a4d655d246ef0768d4e0fa781781f`. Read its
`docs/pi_vehicle_state_validation_2026-09-27.md` using git show.
PC HEAD was `56ec470`, with a clean working tree at the start of this follow-up.
No checkout, merge, reset or cherry-pick was performed. Pi runtime safeguards
remain on the fetched branch; this task does not silently integrate them.

## PC runtime evidence

Installed client links `/usr/local/lib/libvsomeip3.so.3`, pkg-config version 3.7.6.
PC routes Pi 192.168.137.2 and SD 224.244.224.245 through eth0, source 192.168.137.1.
Initial attempt received no SD and timed out; after user confirmed Pi provider
running and other PC clients stopped, the following tests passed:

1. Baseline temporary config without diagnosis: method returned success but logged
   `Invalid client id` three times, one per response.
2. Updated PC launcher continuous subscription ran over 12 seconds, received 13
   events in cyclic order 40/1800/71 -> 60/2400/72 -> 20/1200/70, and stayed alive.
3. Sent SIGINT (the Ctrl+C signal) to the launcher PID, which execs the client.
   Exit code 0; /proc/PID no longer existed. No stale subscriber remained.
4. Immediately ran method launcher: exit 0, 60.00 km/h, 2400 rpm, 72 C;
   zero `Invalid client id` messages.
5. Runtime environment read from /proc:
   `VSOMEIP_APPLICATION_NAME=vehicle-client`,
   `VSOMEIP_CONFIGURATION=/home/jiho/Portfolio/SOMEIP-CAN_Project/config/someip/client_pc.json`.

Commands (repository root, sequential; Pi server must stay running):

```sh
./scripts/run_vehicle_client.sh --someip-profile pc subscribe
# After 12+ seconds, Ctrl+C
./scripts/run_vehicle_client.sh --someip-profile pc method
```

Temporary logs: `/tmp/pc-stream-validation-20260927/`:
`baseline-live.log`, `continuous-live.log`, `method-after-live.log`,
`environment-live.txt` (only VSOMEIP variables). These files are not durable artifacts.

## Invalid client id diagnosis and correction

Installed-source inspection at
`/home/jiho/someip/implementation/routing/src/routing_manager_impl.cpp`,
`routing_manager_impl::is_valid_client_id`, shows response validation compares
`(client_id & diagnosis_mask) >> 8` with `diagnosis`. Client ID is 0x6302;
its masked prefix is 0x63. Previous PC profiles did not specify diagnosis.
The source default is 0x01 unless overridden at build time; runtime A/B testing
above confirms the previous effective configuration rejected the prefix.
This diagnostic logs without discarding the successful responses in this version.

Both `client_pc.json` and compatibility `client_office.json` now explicitly use
`"diagnosis": "0x63"`, `"diagnosis_mask": "0xFF00"`. Client/service/event IDs,
addresses, provider configs and loopback profiles are unchanged.
Do not blindly copy PC diagnosis 0x63 to Pi: remote request validation expects
a different host diagnosis. Future multiple-host/client-ID allocation needs a
consistent, unique host-prefix plan, especially if Pi begins originating requests.

## Tests and remaining limitations

Python/integration: 16 passed, 6 skipped (PC has no vCAN). CTest: 4 passed.
Profile regression checks static client-ID/diagnosis consistency. diff check passed.
The actual PC continuous shutdown and subsequent method regression pending in
98f203c are now verified. Pi capture evidence remains attributed to its report;
no new PC Wireshark capture was taken in this follow-up.

Remaining runtime warnings: TCP `Could not bind to device eth0` (connection still
worked via the OS route), method requested-reliability fallback to available endpoint,
and some socket EOF/close warnings during shutdown. They are not claimed resolved.
No broad permission changes or unrelated transport redesign was made.
Multi-subscriber, multicast events and long-duration recovery remain unverified.
