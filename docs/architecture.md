# Architecture

Planned flow: virtual ECU → configurable SocketCAN interface → CAN gateway → vehicle service layer → SOME/IP provider → SOME/IP client → high-level vehicle API → AI agent and dashboard.

The vehicle service layer converts decoded signals into stable vehicle concepts. CAN identifiers belong in dedicated configuration or DBC files. The AI layer depends on high-level vehicle APIs and has no direct CAN or SOME/IP dependency.

This document will be expanded before component implementation.
