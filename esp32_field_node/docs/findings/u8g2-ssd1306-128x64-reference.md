# U8g2 SSD1306 128×64 ESP32 reference for esp32_field_node

> Created: 2026-05-08
> Source: Explore agent end-to-end read of GPS_OLED_091, DWS1000_UWB
>         display module, and ESP32 Arduino core variants
> Status: Ready to drop into `lib/display/`

Drop-in code for **M2**. Adapted from working ESP32 demos. The user's
display is the 0.96" 128×64 variant; existing demos targeted 0.91"
128×32 — the constructor is the change.

---

## 1. Constructor selection

**Use:** `U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0)`

- `_F_` = full framebuffer — supports partial updates, bitmaps, animations
- `_HW_I2C` = ESP32 hardware I2C peripheral (faster, less CPU than SW bit-bang)
- `NONAME` = unbranded modules — DSD Tech HW-150 / HW-699 are this family
- Default Wire pins (SDA=GPIO21, SCL=GPIO22) — do **not** pass them to
  the constructor when using HW_I2C

**Text-only fallback** (`U8X8_SSD1306_128X64_NONAME_HW_I2C`) saves ~10 KB
flash / ~1 KB RAM if budget gets tight. Currently we have ~70%+ free, so
stick with `U8G2`.

---

## 2. Drop-in display module

`lib/display/display.h`:

```cpp
#ifndef DISPLAY_H
#define DISPLAY_H

#include <U8g2lib.h>
#include <Wire.h>

// SSD1306 128x64 (DSD Tech 0.96", I2C 0x3C). Full framebuffer, hardware I2C,
// default Wire pins (SDA=GPIO21, SCL=GPIO22).
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// Initialize display. Returns false if begin() fails.
bool display_init();

// Two-line boot banner (line2 optional).
void display_banner(const char* line1, const char* line2 = nullptr);

// Single-line status (centred vertically).
void display_status(const char* status_line);

// Top-N WiFi scan results: SSID + small RSSI bar. count capped at 5.
void display_wifi_list(const char** ssids, const int* rssi, int count);

// Clear and flush.
void display_clear();

#endif
```

`lib/display/display.cpp`:

```cpp
#include "display.h"
#include <string.h>

// HW I2C, default pins (SDA=21, SCL=22)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

bool display_init() {
    if (!u8g2.begin()) return false;
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    u8g2.sendBuffer();
    return true;
}

void display_clear() {
    u8g2.clearBuffer();
    u8g2.sendBuffer();
}

void display_banner(const char* line1, const char* line2) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    if (line1) u8g2.drawStr(0, 25, line1);
    if (line2) u8g2.drawStr(0, 40, line2);
    u8g2.sendBuffer();
}

void display_status(const char* status_line) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    if (status_line) u8g2.drawStr(0, 32, status_line);
    u8g2.sendBuffer();
}

void display_wifi_list(const char** ssids, const int* rssi, int count) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "WiFi Scan");

    if (count > 5) count = 5;   // 6 lines total: 1 header + 5 networks
    for (int i = 0; i < count; i++) {
        int y = 20 + i * 10;
        char ssid_buf[21] = {0};
        if (ssids && ssids[i]) {
            strncpy(ssid_buf, ssids[i], sizeof(ssid_buf) - 1);
        }
        u8g2.drawStr(0, y, ssid_buf);

        if (rssi) {
            int v = rssi[i];
            int pct = (v >= -30) ? 100 : (v <= -80 ? 0 : ((v + 80) * 100) / 50);
            int bar = (pct * 6) / 100;
            u8g2.drawFrame(120, y - 8, 8, 8);
            u8g2.drawBox  (122, y - 8, bar, 8);
        }
    }
    u8g2.sendBuffer();
}
```

---

## 3. main.cpp integration sketch

```cpp
#include "display.h"

void setup() {
    Serial.begin(115200);
    delay(100);
    Wire.begin();                      // claim I2C peripheral

    if (!display_init()) {
        Serial.println(F("[OLED] init failed — check 0x3C, SDA=21, SCL=22"));
    } else {
        Serial.println(F("[OLED] ready"));
    }
    display_banner("SwarmLoc node", "M2 boot");
    delay(1500);
    // ... continue with I2C scan, WiFi scan, etc.
}
```

After a WiFi scan completes:

```cpp
display_wifi_list(ssid_array, rssi_array, n);
```

---

## 4. Pin / wiring confirmation

| Signal | ESP32 GPIO | Note |
|--------|------------|------|
| SDA | 21 | ESP32 default I2C data, not overridden |
| SCL | 22 | ESP32 default I2C clock, not overridden |
| VCC | 3V3 | DSD Tech SSD1306 is 3.3V; do **not** use 5V |
| GND | GND | — |

**Important**: the existing `GPS_OLED_091.ino` demo uses GPIO23 for SDA
because it targets a Feather ESP32 with a non-standard pinout. The
DevKit-V1 / WROOM-32D we have follows the canonical 21/22. Do not copy
the GPS demo's pin numbers.

I2C address selection: SSD1306 ships at **0x3C** with the ADDR pad
grounded; soldering the ADDR jumper makes it 0x3D. The bootstrap I2C
scan in `src/main.cpp` reports the actual address found.

---

## 5. Surprises worth knowing

1. **Don't pass SDA/SCL to HW_I2C constructor.** The SW_I2C variants take
   pins as arguments; HW_I2C variants pull them from `Wire.begin()`. The
   GPS_OLED_091 demo uses SW_I2C with explicit pins — don't copy that.

2. **`Wire.begin()` before `display_init()`.** The I2C peripheral must be
   claimed first.

3. **No power-on delay needed on ESP32.** GPS demo had a 200 ms safety
   margin; not required — bootloader handles power sequencing.

4. **Font choice**: `u8g2_font_6x10_tf` fits ~21 chars per line and gives
   6 lines on 128×64. `u8g2_font_4x6_tf` is denser but harder to read.

5. **Memory budget**: full framebuffer = 128×64/8 = 1024 bytes RAM, plus
   library overhead. ESP32 has 327 KB free RAM after FreeRTOS — completely
   negligible.

6. **0x3D address**: if the I2C scanner reports 0x3D instead of 0x3C, the
   board has the ADDR jumper bridged. U8g2 can be told via
   `u8g2.setI2CAddress(0x3D << 1)` (the `<< 1` is because U8g2 wants the
   8-bit form of the 7-bit address). Not needed by default.

---

## 6. References cited (full paths)

- `/home/devel/SwarmLoc/GPS_module/GPS_OLED_091/GPS_OLED_091.ino`
- `/home/devel/SwarmLoc/DWS1000_UWB/include/display.h`
- `/home/devel/SwarmLoc/DWS1000_UWB/include/config.h`
- `/home/devel/SwarmLoc/esp32_field_node/platformio.ini`
- `/home/devel/SwarmLoc/esp32_field_node/src/main.cpp`
- ESP32 framework variant `pins_arduino.h` (SDA=21, SCL=22 confirmed)
