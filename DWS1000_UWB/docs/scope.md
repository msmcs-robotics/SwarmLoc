# DWS1000_UWB - Scope

> Last updated: 2026-01-17
> Status: Active

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
- [ ] TWR ranging protocol produces distance measurements
- [ ] Measurements are consistent and repeatable
- [ ] Serial output shows distance in human-readable format

### Technical Requirements
*Technical constraints, compatibility, performance needs*

- [x] Use DW1000 chip (Device ID: DECA0130), NOT DWM3000
- [x] PlatformIO-based build system
- [ ] Interrupt-driven communication (bug fix applied)
- [ ] Target accuracy: ±10-20 cm after calibration

### Resource Requirements
*Hardware, software, dependencies, services*

- [x] 2x Arduino Uno (ATmega328P)
- [x] 2x Qorvo PCL298336 v1.3 DWS1000 shields
- [x] USB cables for programming and monitoring
- [x] arduino-dw1000 library (in lib/)

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
- [VERIFIED] Interrupt bug in DW1000.cpp has been fixed
- [ASSUMED] Physical connections and antennas are properly seated

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

**Library Flexibility**: Open to using multiple different libraries and editing them as needed to understand the specific DWS1000 module. The goal is to learn how this hardware works, not to stick rigidly to one library. Experimentation and modification of library code is encouraged.

## Technical Decisions

| Decision | Choice | Rationale | Date |
|----------|--------|-----------|------|
| Library | arduino-dw1000 v0.9 (editable) | Starting point, can modify or replace | 2026-01-08 |
| Build system | PlatformIO | Multi-environment support | 2026-01-08 |
| Pin config | RST=9, IRQ=2, CS=10 | Shield default | 2026-01-08 |
| Library source | Copied to lib/ (not submodule) | Allows direct editing | 2026-01-11 |

## Integration Points

- USB serial for programming and monitoring
- SPI bus between Arduino and DW1000 shield
- U.FL antenna connectors on shields

## Open Questions

- [ ] Is D8→D2 jumper wire still needed or should it be removed?
- [x] Why isn't receiver detecting transmissions? → Physical connection suspected

## Critical Notes

- **Jumper wire D8→D2** currently installed - may need removal for default pin config
- **Interrupt bug** in DW1000.cpp was fixed (buffer overrun in interruptOnReceiveFailed)
- **USB hub issue** resolved by moving ACM1 to direct USB port

---

## Revision History

| Date | Changes | By |
|------|---------|-----|
| 2026-01-17 | Reformatted to match template | LLM |
| 2026-01-17 | Initial scope creation | LLM |

---

*This document evolves as the project develops. Requirements, constraints, and boundaries can be added, modified, or removed as understanding improves.*
