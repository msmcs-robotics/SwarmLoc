# esp32_field_node — Scope

> Last updated: 2026-05-08
> Status: Draft — initial bootstrap, hardware not yet wired

---

## Overview

ESP32-based field node for the SwarmLoc system. Provides a hardware platform
for on-site WiFi probing, status display, and inertial sensing. The **first**
near-term goal is to use the ESP32 itself as a probe to characterize the
"MSC guest" WiFi (encryption type and auth flow) before deciding how to
connect to it.

## Objectives

- Build a reusable ESP32 firmware base with display + WiFi + IMU scaffolding
- Provide a WiFi scanner that lists nearby SSIDs and reports their security types
- Connect the node to "MSC guest" WiFi once the auth type is identified
- Read and display IMU data (MPU6050) on the SSD1306 OLED

## Requirements

### Functional Requirements
*What the system must do*

- [ ] Boot, identify chip, scan I2C bus, confirm OLED at 0x3C
- [ ] Scan WiFi and print SSID, RSSI, channel, encryption type to serial
- [ ] Connect to a configured WiFi network (auth type TBD per scan results)
- [ ] Render status (SSID, IP, RSSI) on the SSD1306 OLED
- [ ] Read MPU6050 acceleration + gyro and stream to serial / OLED

### Technical Requirements
*Technical constraints, compatibility, performance needs*

- [ ] Build with PlatformIO (`platform = espressif32`, `board = esp32dev`)
- [ ] Arduino framework (matches sibling projects, fast bootstrap)
- [ ] Free, open libraries only (olikraus/U8g2, Adafruit_MPU6050)
- [ ] No paid services, no cloud dependencies

### Resource Requirements
*Hardware, software, dependencies*

- [x] 1x ESP32-WROOM-32D DevKit (CP2102 USB-UART → /dev/ttyUSB0)
- [x] 1x DSD Tech 0.96" SSD1306 OLED (I2C, addr 0x3C)
- [ ] 1x MPU6050 breakout (deferred to M4)
- [ ] Jumper wires + breadboard

## Constraints

| Constraint | Reason | Flexible? |
|------------|--------|-----------|
| ESP32-WROOM-32D | Hardware on hand | No |
| Arduino framework | Matches sibling projects | Maybe (could move to ESP-IDF for advanced WPA2-Enterprise APIs if needed) |
| MSC guest WiFi as target network | Deployment site | No |
| Single ESP32 on /dev/ttyUSB0 | Only board currently plugged in | Yes |

## Assumptions

- [ASSUMED] DevKit pinout: SDA=GPIO21, SCL=GPIO22 (ESP32 default)
- [ASSUMED] OLED is 128x64 at I2C addr 0x3C (HW-150 / HW-699 variant)
- [REVISED 2026-05-08, post-M1-scan] **`MSC - GUEST` is OPEN, not
  Enterprise.** Live scan from the ESP32 reports `auth=OPEN` on both
  channel-6 and channel-11 BSSIDs. Almost certainly a captive portal
  for guest auth. M3 primary path is now: connect open → probe captive
  portal via HTTP/204 endpoint → display result. See
  `findings/msc-guest-network-characterization.md`. The original
  WPA2-Enterprise PEAP path is retained for `UAA WiFi -MatSu` (which
  IS Enterprise per the same scan).

## Boundaries

### In Scope

- Firmware for an ESP32-WROOM-32D with SSD1306 + (later) MPU6050
- WiFi scanning and characterization tooling
- WiFi station-mode connection (open / WPA2-PSK / WPA2-Enterprise as required)
- Captive-portal detection (HTTP probe) — only enough to recognize one
- Status display on OLED
- IMU read-out and display
- Findings docs for re-discovered tricks (e.g. WPA2-Enterprise cert handling)

### Out of Scope (Exclusions)

- UWB ranging (handled by sibling [DWS1000_UWB/](../../DWS1000_UWB/))
- LoRa radio (handled by sibling [lora_feather_esp32/](../../lora_feather_esp32/))
- GPS (handled by sibling [GPS_module/](../../GPS_module/))
- WiFi attacks, monitor mode, packet injection, deauth
- Captive-portal credential automation beyond what the user explicitly requests
- Multi-node mesh / coordination — that belongs at the parent SwarmLoc level
- ESP-IDF native build, OTA updates, MQTT (potential future, not committed)

## Technical Decisions

| Decision | Choice | Rationale | Date |
|----------|--------|-----------|------|
| Board | esp32dev (ESP32-WROOM-32D) | User-confirmed chip marking | 2026-05-08 |
| Framework | Arduino | Matches sibling projects | 2026-05-08 |
| Display library | olikraus/U8g2 (text + graphics) | Sibling projects already use it; existing demos available — see findings/existing-demo-inventory.md | 2026-05-08 |
| Project location | sibling of DWS1000_UWB at repo root | Embedded-projects pattern | 2026-05-08 |
| First work item | WiFi scanner, NOT auth | MSC type still to be confirmed empirically | 2026-05-08 |
| Reuse before rebuild | Demos found on filesystem: `GPS_module/GPS_OLED_091.ino` for OLED, `GravityProbe/esp32_ewpa2_iic_091/` for PEAP | Avoid re-fighting cert handling | 2026-05-08 |
| WPA2-Enterprise auth | PEAP/MSCHAPv2, **no client certs** | User confirmed MSC guest accepts username+password without certs | 2026-05-08 |

## Integration Points

- USB serial (/dev/ttyUSB0) for programming, monitoring, and a serial CLI
- I2C bus (SDA=21, SCL=22) for OLED and MPU6050
- WiFi station for on-site network connection

## Open Questions

- [ ] What encryption / auth type does "MSC guest" actually use?
  → Resolved by M1 scanner output (`findings/msc-guest-network-characterization.md`)
- [x] Are there reusable demos for SSD1306 and WPA2-Enterprise on this system?
  → YES, see `findings/existing-demo-inventory.md`. Display:
  `GPS_module/GPS_OLED_091.ino` (ESP32+U8g2) and `DWS1000_UWB/include/display.h`
  (U8x8 wrapper). WPA2-Enterprise: `floppi/flight_controller/`
  (production-grade) and `GravityProbe/esp32_ewpa2_iic_091/` (campus PEAP).

## Critical Notes

- The parent repo's `docs/` folder is intentionally sparse and we do not
  modify it. This project's docs live entirely under
  `esp32_field_node/docs/`.
- WPA2-Enterprise on ESP32 historically uses `esp_wpa2.h` (older) or
  `esp_eap_client.h` (newer ESP-IDF >= 5.x). Confirm which the installed
  Arduino-ESP32 core ships with before writing auth code.
- **MSC guest specifically** does NOT require client certs (user confirmed
  2026-05-08). Use PEAP/MSCHAPv2 with username + password; the canonical
  PIO `WiFiClientEnterprise` example runs without certs by default. See
  `findings/msc-guest-wifi-no-certs.md` for full notes.

---

## Revision History

| Date | Changes | By |
|------|---------|-----|
| 2026-05-08 | Initial scope, M0 project bootstrap | LLM |
