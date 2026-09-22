# Interfaces

## Milestone 1 CAN frame contract

The virtual ECU sends one standard (11-bit identifier) CAN data frame through the configured SocketCAN interface. For this temporary test slice, the identifier is `0x100`, the DLC is 8, and the payload is `00 01 02 03 04 05 06 07`. The sender repeats the same frame at a configurable positive interval. The default interface is `vcan0`.

The C++ receiver reads standard CAN data frames and writes one line per frame to stdout:

```text
YYYY-MM-DDTHH:MM:SS.mmmZ ID=0x100 DLC=8 DATA=00 01 02 03 04 05 06 07
```

The timestamp is the receiver's UTC system time when it processes the frame, not a bus or ECU timestamp. The receiver ignores extended, remote, and error frames. Errors go to stderr and yield a nonzero exit status.

`0x100` and its payload are temporary demonstration values. Permanent CAN identifiers and signal definitions will move to `config/dbc/` or `config/can/`. This milestone does not decode signals or define vehicle service, SOME/IP, or AI interfaces.
