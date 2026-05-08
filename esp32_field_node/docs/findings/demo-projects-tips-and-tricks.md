# Demo project tips & tricks — ESP32 + WiFi + OLED + WPA2-Enterprise

> Created: 2026-05-08
> Source: Agent F (Explore) read across `~/floppi/`, `~/GravityProbe/`,
>         `~/SwarmLoc/`, and PIO core examples
> Purpose: hard-won knowledge from existing demos, distilled. Reused for M3.

---

## 1. Captive-portal handling

**No existing ESP32 code in any of the user's repos implements a full
captive-portal detector.** floppi has design notes but no implementation.
We are implementing this fresh in `lib/wifi_field/wifi_field.cpp`.

The standard probe (used by Android, iOS, NetworkManager):

```
GET http://connectivitycheck.gstatic.com/generate_204
  204 No Content       → online, no portal
  200 OK + body        → captive portal
  30x Location: <url>  → captive portal (portal_url in Location header)
```

Alternative probes:
- `http://detectportal.firefox.com/canonical.html` (expects body "success")
- `http://www.msftconnecttest.com/connecttest.txt` (expects "Microsoft Connect Test")
- `http://neverssl.com` (always plain HTTP by spec)

**Library option not chosen**: `tzapu/WiFiManager` — too heavy; would block
during setup and require a serving AP. We do passive detection only.

---

## 2. ESP32 WiFi gotchas

### Mandatory `WiFi.disconnect(true)` before `WiFi.begin()`

All four production demos (GravityProbe x2, floppi, PIO canonical) do:

```cpp
WiFi.disconnect(true);  // true = turn off radio AND clear state
delay(100);
WiFi.mode(WIFI_STA);
delay(100);
WiFi.begin(...);
```

**Why**: stale WPA state from prior connections causes silent failures.
Omitting the `true` leaves the radio in an unknown state.

### Auto-reconnect is unreliable

`WiFi.setAutoReconnect(true)` exists but behaves inconsistently across
ESP32 silicon revisions. **floppi pattern** (production): manual periodic
`WiFi.reconnect()` in the loop, every 5 s while disconnected. This is
what `esp32_field_node` does in `loop()`.

### Never block the main loop on connection

```cpp
// WRONG — freezes everything
while (WiFi.status() != WL_CONNECTED) delay(500);

// RIGHT — bounded wait + abort
unsigned long start = millis();
while (WiFi.status() != WL_CONNECTED) {
  if (millis() - start >= timeout_ms) break;
  delay(250);
}
```

Our `wifi_field.cpp::wait_connected()` does the bounded version.

### WiFi scan while connected is risky

Scans on the same radio that's holding an association can hang or drop
the connection. **Best practice**: only `WiFi.scanNetworks()` when
not connected, OR explicitly `disconnect(true)` first and reconnect after.
M3's `loop()` skips the auto-scan when `g_connected` is true.

---

## 3. U8g2 / SSD1306 OLED gotchas

(Already captured in `u8g2-ssd1306-128x64-reference.md`. Cross-references:)

- **Use `_F_HW_I2C`** constructor — full framebuffer, hardware I2C, no
  pin args (Wire.begin claims them).
- **Don't copy GPS_OLED_091's pins** — that demo uses Feather-specific
  GPIO23 SDA. We use the canonical ESP32 pins (21/22).
- **0x3C** is the default; jumpering ADDR moves to 0x3D — handle via
  `u8g2.setI2CAddress(0x3D << 1)` if needed.
- **`u8g2_font_6x10_tf`** fits ~21 chars/line, 6 lines, 128×64 — our pick.
- **Wire.begin() before display_init()** — we do this.

---

## 4. State-machine / dual-core (production from floppi)

For our current scope (M3) we don't need dual-core; one core is plenty
for WiFi + OLED. Capturing the pattern in case we later add IMU / MQTT:

```cpp
xTaskCreatePinnedToCore(flightTask, "FC",   4096, NULL, 2, NULL, 0);
xTaskCreatePinnedToCore(wifiTask,   "WiFi", 8192, NULL, 1, NULL, 1);
```

- Core 0 = real-time / sensor / IMU loop (deterministic)
- Core 1 = WiFi, HTTP, OLED, display refresh
- Mutex (`SemaphoreHandle_t`) to share sensor data between cores

Stack-size guidance from floppi:
- Sensor task: 4096
- WiFi + HTTP task: 8192
- Display task: 4096

Source: `~/floppi/flight_controller/docs/findings/esp32-dual-core-research.md`

---

## 5. PlatformIO / Arduino-ESP32 toolchain

### Pin the platform if you can

Currently `platform = espressif32` (unpinned) → resolves to Arduino-ESP32
v3.20017.241212 (Dec 2024 build, IDF 5.x-based). To freeze:

```ini
platform = espressif32@6.7.0
```

We're leaving it unpinned for now since builds are reproducible enough on
this dev machine; pin if/when we share the project across machines.

### `monitor_filters = esp32_exception_decoder, time` is mandatory

Without it, an ESP32 panic dumps raw addresses; with it, you get source
locations and timestamps. Already in our `platformio.ini`.

### Vendoring vs. registry libs

floppi vendors `ESPAsyncWebServer`, `AsyncTCP`, `U8g2` in `lib/`. We use
the registry for U8g2. If we ever lose internet at the dev machine,
vendor U8g2 to keep builds working offline.

---

## 6. Other surprises

### Persisting credentials in NVS (Preferences)

Future enhancement — store WiFi creds in NVS instead of compiling them
in:

```cpp
#include <Preferences.h>
Preferences prefs;
prefs.begin("wifi", false);
prefs.putString("ssid", ssid);
prefs.putString("pass", pass);
prefs.end();
```

Out of scope for M3 — we use compile-time defines via
`include/wifi_credentials.h` + `.local.h` override. NVS is a future "set
credentials over serial" feature.

### Power consumption

| State | Current |
|-------|---------|
| TX active | 160–260 mA |
| RX active | 95–100 mA |
| Modem sleep | 20–25 mA @ 80 MHz |
| WiFi off | 20 mA |

For battery operation, `esp_wifi_set_ps(WIFI_PS_MAX_MODEM)` saves a lot.
Out of scope here — we run from USB.

### RSSI quality reference

| RSSI (dBm) | Quality |
|------------|---------|
| -30 to -50 | Excellent |
| -50 to -60 | Good |
| -60 to -70 | Fair |
| -70 to -80 | Weak |
| < -80 | Poor |

Our M1 scan saw -74 to -80 dBm for MSC - GUEST and UAA WiFi -MatSu —
"weak" but workable.

### Event-based reconnection (advanced, not used yet)

Instead of polling `WiFi.status()`, register a callback:

```cpp
void onWifiDisconnect(WiFiEvent_t e, WiFiEventInfo_t info) {
    Serial.println("[wifi] disconnected, retrying...");
    WiFi.begin(ssid, pass);
}
WiFi.onEvent(onWifiDisconnect, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
```

Cleaner than the polling we currently do in `loop()`. Future refactor.

---

## Files cited (full paths)

- `/home/devel/floppi/flight_controller/docs/findings/esp32-wifi-connectivity.md`
- `/home/devel/floppi/flight_controller/docs/findings/esp32-dual-core-research.md`
- `/home/devel/floppi/flight_controller/src/wifi_manager.cpp`
- `/home/devel/floppi/flight_controller/platformio.ini`
- `/home/devel/GravityProbe/esp32_ewpa2_iic_091/esp32_ewpa2_iic_091.ino`
- `/home/devel/GravityProbe/esp32_ent_wpa2_peap_web80/esp32_ent_wpa2_peap_web80.ino`
- `/home/devel/GravityProbe/esp32.md` (hardware pinout reference)
- `/home/devel/SwarmLoc/GPS_module/GPS_OLED_091/GPS_OLED_091.ino`
- `/home/devel/SwarmLoc/DWS1000_UWB/include/display.h`

---

## What we apply in M3

1. **Captive-portal probe** — gstatic /generate_204 (already in `wifi_field.cpp`)
2. **Mandatory `WiFi.disconnect(true)` before begin** — done in both connect helpers
3. **Bounded `wait_connected()` with timeout** — done
4. **Manual reconnect every 5 s in loop** — done in main.cpp loop
5. **Skip auto-rescan when connected** — done
6. **Default to OPEN path when no enterprise creds** — done via WIFI_ENT_USERNAME == "FILL_ME_IN" check
