# Existing demo inventory — display + WPA2-Enterprise

> Created: 2026-05-08
> Source: Two parallel Explore agents run during M0 bootstrap
> Status: Initial sweep complete

This file catalogs reusable code already on the user's filesystem so we
**copy and adapt** rather than write from scratch. Captured before any
M2 / M3 implementation work begins.

---

## SSD1306 OLED — for M2

### Top picks

**1. `GPS_module/GPS_OLED_091/GPS_OLED_091.ino`** — closest match for ESP32
- Framework: Arduino, target board: Adafruit Feather ESP32
- Library: **U8g2** (graphics mode, full framebuffer)
- Display: DSDTECH 0.91" SSD1306 **128×32** (NOT 128×64), I2C on GPIO23/22
- Pattern:
  ```cpp
  U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, SCL, SDA);
  u8g2.begin();
  u8g2.drawStr(0, 10, "text");
  u8g2.sendBuffer();
  ```
- **Adapt for esp32_field_node**: swap constructor for the 0.96" 128×64
  variant (`U8G2_SSD1306_128X64_NONAME_F_HW_I2C`) and use ESP32 default
  I2C pins SDA=GPIO21, SCL=GPIO22.

**2. `DWS1000_UWB/include/display.h`** — modular U8x8 wrapper
- Target: Arduino Uno (AVR), but the wrapper pattern ports cleanly
- Library: **U8x8** (text mode, no framebuffer — smaller flash)
- Display: DSDTECH 0.91" SSD1306 128×32 I2C
- Why useful: gives us a "drop-in display module with 4 helpers" pattern
  (`display_init()`, `display_message(line, str)`, etc.) — good shape
  for `lib/display/`.

### Other matches

- `lora_feather_esp32/ESP32_I2C_Scanner/ESP32_I2C_Scanner.ino` — bare I2C
  scanner. Useful as a sanity-check baseline if our scanner-in-`main.cpp`
  ever misbehaves.
- `GPS_module/basic_GPS_LAT_LONG_LCD_adafruit_featherwing/` — uses an LCD
  (not OLED), not relevant.
- `/home/devel/Desktop/SwarmLoc/` — mirror of this repo; ignore.

### Library decision

The repo convention is **U8g2 / U8x8** (used by both `DWS1000_UWB` and
`GPS_module`). We follow that — `platformio.ini` declares
`olikraus/U8g2` as the only display dep. No Adafruit_SSD1306. Smaller
flash, established locally.

### Display-size warning

Both existing demos target **128×32** (0.91"). The user's display is
**128×64** (0.96"). Constructor must change:

| Display | U8g2 (graphics) | U8x8 (text) |
|---------|-----------------|--------------|
| 128×32 0.91" | `U8G2_SSD1306_128X32_UNIVISION_*` | `U8X8_SSD1306_128X32_UNIVISION_*` |
| **128×64 0.96"** *(ours)* | `U8G2_SSD1306_128X64_NONAME_*` | `U8X8_SSD1306_128X64_NONAME_*` |

Use the `_HW_I2C` suffix on ESP32 to use the hardware I2C peripheral
(higher throughput than `_SW_I2C`).

---

## WPA2-Enterprise — for M3

### Top picks

**1. `~/floppi/flight_controller/`** — production-grade WiFi manager
- Framework: Arduino (ESP32)
- Auth: PEAP (no certs) **or** EAP-TLS (with certs) — selectable via flags
- Files of interest:
  - `include/wifi_credentials.h` — supports both PSK and Enterprise
  - `include/wifi_certs.h` — optional CA cert + client cert/key for TLS
  - `src/wifi_manager.cpp` — `setupWiFi()` + `handleWiFi()` with
    auto-reconnect, timeouts, dual-core safe
- Quality bar: this is our reference. Adapt the credentials.h pattern even
  if we don't need TLS.

**2. `~/GravityProbe/esp32_ewpa2_iic_091/`** — closest behavioral match
- Framework: Arduino
- Auth: **PEAP + MSCHAPv2**, no client certs
- Tested against: "UAA WiFi - MatSu" (a campus-style WPA2-Enterprise SSID)
- Includes: OLED display of acquired IP — exactly the M3 demo shape we want
- Smallest path to a working MSC-guest connection.

**3. `~/.platformio/packages/framework-arduinoespressif32/libraries/WiFi/examples/WiFiClientEnterprise/`**
- Canonical PIO Arduino-ESP32 reference example
- Auth: PEAP / TTLS / TLS — three commented variants
- Tested across many universities (multiple countries)
- Compiles cleanly — good "is my toolchain healthy?" sanity check.

### Other matches

- `~/GravityProbe/esp32_ent_wpa2_peap_web80/` — PEAP + MSCHAPv2 plus a tiny
  web server. Useful if we ever want a portal/status UI on the node.
- `~/GravityProbe/esp32_enterprise_wpa3_eap/` — WPA2/WPA3 EAP variant.
  Likely not needed unless MSC guest is WPA3-Enterprise.
- `~/.platformio/packages/framework-arduinoespressif32/libraries/WiFiClientSecure/examples/WiFiClientSecureEnterprise/`
  — HTTPS over Enterprise. Stretch goal at best.

### Decision for M3 (MSC guest)

User confirmed (2026-05-08): MSC guest is WPA2-Enterprise but **does not
require client certs**. Path forward:

1. Reuse `GravityProbe/esp32_ewpa2_iic_091/` as the structural template
2. Strip down to PEAP + MSCHAPv2 with username + password only
3. Skip CA cert verification (canonical PIO example does this by default)
4. If connection fails: capture exact failure mode, then borrow code from
   `floppi/flight_controller` (auto-reconnect, timeouts)

### Notable absences

- **No standalone Adafruit_SSD1306 sketches anywhere** — confirms the
  U8g2 convention is repo-wide.
- **No "MSC guest"-specific code** anywhere — first time the user is
  attempting this network from this codebase.
- **Nothing useful in `~/Documents/`, `~/Projects/`, `~/code/`,
  `~/dev/`, `~/workspace/`** — those locations are absent or empty.

---

## Action items derived from this inventory

- [x] Switch `platformio.ini` lib_deps to `olikraus/U8g2`
- [ ] (M2) Copy `GPS_OLED_091.ino` rendering pattern into `src/`,
      adjust constructor for 128×64
- [ ] (M3) Read `GravityProbe/esp32_ewpa2_iic_091/*.ino` end-to-end before
      writing M3 firmware
- [ ] (M3) If anything's unclear, read
      `floppi/flight_controller/src/wifi_manager.cpp` for the production
      reference (auto-reconnect, timeouts, error handling)
