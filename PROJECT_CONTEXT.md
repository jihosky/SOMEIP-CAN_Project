# SOMEIP-CAN Project Context

## Goal

Create a complete portfolio project combining:

- Vehicle software
- CAN communication
- SOME/IP
- Linux
- Raspberry Pi
- AI Agent
- Automated diagnostics

## Current Hardware

- Raspberry Pi 5
- Dual-channel CAN HAT
- PC running Windows + WSL2
- No external physical ECU currently

## Current Design Decision

Development will begin with vCAN.

Reason:
There is currently no independent physical CAN node available for reliable CAN bus testing.

Physical CAN will be introduced later using the Raspberry Pi CAN HAT.

## Planned Architecture

Virtual ECU Simulator
        |
        v
      vcan0
        |
        v
    SocketCAN
        |
        v
 CAN / Signal Decoder
        |
        v
 Vehicle Service Layer
        |
        v
 SOME/IP Service Provider
        |
      Ethernet
        |
        v
 PC-side SOME/IP Client
        |
        v
 AI Diagnostic Agent
        |
        v
 Dashboard / Reports

## Middleware

SOME/IP will be used as the main vehicle service middleware.

Planned concepts:

- SOME/IP methods
- SOME/IP events
- SOME/IP Service Discovery
- VehicleDataService
- DiagnosticService

## AI Design

The AI Agent should not interpret raw CAN frames directly.

The AI should use high-level tools such as:

- get_vehicle_speed()
- get_engine_rpm()
- get_coolant_temperature()
- read_dtc()
- read_did()
- run_diagnostic_test()

The lower layers should handle CAN and SOME/IP communication.

## Future Extensions

- Fault injection
- Anomaly detection
- UDS diagnostics
- RAG-based diagnostic knowledge
- Human approval for sensitive vehicle actions
- Physical CAN verification
- Raspberry Pi deployment
- CI/CD