# Project Instructions

## Project Goal

Build a portfolio-grade AI-assisted automotive software platform.

Target architecture:

Virtual ECU
-> vCAN / SocketCAN
-> CAN Gateway
-> SOME/IP middleware
-> PC-side AI Agent
-> Diagnostic Dashboard

## Development Rules

- Use Linux/WSL as the primary development environment.
- Python is preferred for ECU simulation, AI agents, ML, automation, and tests.
- C++ is preferred for SocketCAN gateway and SOME/IP/vSomeIP components.
- Do not let the AI agent access raw CAN frames directly.
- Vehicle data must pass through a service abstraction layer.
- Avoid hardcoded CAN IDs outside dedicated configuration or DBC files.
- Design the CAN backend so vcan0 can later be replaced by can0/can1.
- New features should include tests.
- C++ changes must build successfully before completion.
- Python changes must pass pytest.
- Do not modify unrelated files.
- Before major architectural changes, explain the impact first.
- Keep the architecture modular and hardware-independent.

## Current Priority

Do not implement vehicle features yet.

First:
1. establish the repository structure,
2. document architecture,
3. prepare build/test infrastructure,
4. verify the AI-assisted development workflow.