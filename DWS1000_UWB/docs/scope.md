# DWS1000_UWB - Scope

> Last updated: 2026-02-27
> Status: Active — nearing completion

---

## Overview

UWB-based ranging system using two Arduino Unos with DWS1000 hat modules (Qorvo PCL298336) to establish bidirectional communication and measure distance using Two-Way Ranging (TWR) protocol.

## Objectives

- Establish reliable UWB communication between two Arduino Unos
- Implement Two-Way Ranging (TWR) protocol for distance measurement
- Achieve reasonable accuracy in distance measurements
- Document findings, configuration, and lessons learned

## Requirements

### Functional Requirements
*What the system must do*

- [x] Both devices can send and receive UWB frames
- [x] TWR ranging protocol produces distance measurements
- [x] Measurements are consistent and repeatable
- [x] Serial output shows distance in human-readable format

### Technical Requirements
*Technical constraints, compatibility, performance needs*

- [x] Use DW1000 chip (Device ID: DECA0130), NOT DWM3000
- [x] PlatformIO-based build system
- [x] Interrupt-driven communication (DW1000-ng IRQ handlers)
- [x] Target accuracy: ±10-20 cm after calibration (achieved +4.6 cm error)

### Resource Requirements
*Hardware, software, dependencies, services*

- [x] 2x Arduino Uno (ATmega328P)
- [x] 2x Qorvo PCL298336 v1.3 DWS1000 shields
- [x] USB cables for programming and monitoring
- [x] DW1000-ng library (in lib/DW1000-ng/)

## Constraints

| Constraint | Reason | Flexible? |
|------------|--------|-----------|
| Arduino Uno MCU | Hardware on hand | No |
| DW1000 chip | Hardware identification confirmed | No |
| Two-node only | Scope limitation per user | No |
| No paid services | Budget | No |
| Linux host | User environment | No |

## Assumptions

- [VERIFIED] Hardware is DW1000, not DWM3000 (Device ID: DECA0130)
- [VERIFIED] USB ports: /dev/ttyACM0, /dev/ttyACM1
- [VERIFIED] J1 jumper on pins 1-2 required (DC-DC → DWM1000)
- [VERIFIED] D8→D2 wire required (IRQ routing to INT0)
- [VERIFIED] DW1000-ng library works; thotro library RX broken

## Boundaries

### In Scope

- Firmware for initiator (tag) and responder (anchor)
- Bidirectional UWB communication
- Distance measurements using TWR
- Documentation of configuration and findings
- Antenna delay calibration

### Out of Scope (Exclusions)

> Prevents scope creep. Everything outside primary goal.

- GPS/GNSS integration
- Visual SLAM or camera-based positioning
- Specific drone platform integration
- Multi-node swarm coordination (beyond two nodes)
- Commercial deployment / production-ready code
- ESP32 migration (unless Arduino Uno proves completely inadequate)
- Multilateration positioning algorithms
- Collision avoidance systems

## Development Approach

**Library**: DW1000-ng (better PLL init, PLLLDT, clock sequencing). thotro DW1000 library deprecated — RX broken, incompatible frame format.

## Technical Decisions

| Decision | Choice | Rationale | Date |
|----------|--------|-----------|------|
| Library | DW1000-ng | Proper PLL init, working RX | 2026-02-27 |
| Build system | PlatformIO | Multi-environment support | 2026-01-08 |
| Pin config | RST=7, IRQ=2 (via D8→D2 wire), CS=10 | Shield + INT0 routing | 2026-01-08 |
| Library source | Copied to lib/ (not submodule) | Allows direct editing | 2026-01-11 |
| J1 jumper | Pins 1-2 (DC-DC → DWM1000) | Root cause of RX failure | 2026-02-27 |
| TWR config | 850kbps, 16MHz PRF, Ch5, Preamble 256 | Best balance of speed/reliability | 2026-02-27 |

## Integration Points

- USB serial for programming and monitoring
- SPI bus between Arduino and DW1000 shield
- U.FL antenna connectors on shields

## Open Questions

- [x] Is D8→D2 jumper wire needed? → YES, required for IRQ routing
- [x] Why isn't receiver detecting transmissions? → J1 jumper missing (power floating)

## Critical Notes

- **J1 jumper on pins 1-2** is REQUIRED — without it DWM1000 power rail floats
- **D8→D2 wire** required on each shield for IRQ routing to Arduino INT0
- **DW1000-ng library** required — thotro library has broken RX

---

## Revision History

| Date | Changes | By |
|------|---------|-----|
| 2026-02-27 | Updated: all requirements met except calibration, DW1000-ng selected, J1 jumper required | LLM |
| 2026-01-17 | Reformatted to match template | LLM |
| 2026-01-17 | Initial scope creation | LLM |

---

*This document evolves as the project develops. Requirements, constraints, and boundaries can be added, modified, or removed as understanding improves.*
