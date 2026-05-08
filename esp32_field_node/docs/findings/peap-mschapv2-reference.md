# PEAP / MSCHAPv2 no-cert reference for esp32_field_node

> Created: 2026-05-08
> Source: Explore agent end-to-end read of four working PEAP demos +
>         Arduino-ESP32 core inspection
> Status: Ready to drop into `lib/wifi_enterprise/`

This is the load-bearing reference for **M3**. Pulled from working code on
this machine, NOT from web docs. The user already fought through these
demos once — this captures what they learned.

---

## 1. Headers and what core they imply

All four working demos use **`esp_wpa2.h`** (the legacy header):

- `~/GravityProbe/esp32_ewpa2_iic_091/` — uses `WiFi.begin(ssid, WPA2_AUTH_PEAP, ...)`
- `~/GravityProbe/esp32_ent_wpa2_peap_web80/` — same
- `~/.platformio/.../WiFiClientEnterprise/WiFiClientEnterprise.ino` —
  explicitly `#include "esp_wpa2.h"`
- `~/floppi/flight_controller/src/wifi_manager.cpp` — uses `WPA2_AUTH_PEAP`
  / `WPA2_AUTH_TLS` macros

**Conclusion**: esp32_field_node uses **`esp_wpa2.h`**.

The current `platform = espressif32` (unpinned) resolves to **Arduino-ESP32
v3.20017.241212** (IDF 5.x-based, Dec 2024 build). The legacy
`esp_wpa2.h` is still present in the SDK at
`~/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32/include/wpa_supplicant/esp_supplicant/include/esp_wpa2.h`
and the `WiFi.begin(ssid, auth_method, ...)` overload remains stable.

If the core upgrades to one that drops the legacy header, switch to
`esp_eap_client.h` per the IDF 5.x naming.

---

## 2. Minimum-viable PEAP no-cert connect

```cpp
// lib/wifi_enterprise/wifi_enterprise.cpp
#include <WiFi.h>
#include "esp_wpa2.h"

// Connect to a WPA2-Enterprise PEAP/MSCHAPv2 network without certificate
// verification. Minimum viable for guest-class networks (e.g. "MSC guest")
// that accept only username + password.
//
// Returns true if WL_CONNECTED reached within timeout_ms.
bool wifi_enterprise_connect(
    const char* ssid,
    const char* identity,
    const char* user,
    const char* pass,
    uint32_t    timeout_ms
) {
    if (!ssid || !identity || !user || !pass) {
        Serial.println(F("[WiFi] null credential pointer"));
        return false;
    }

    Serial.print(F("[WiFi] Connecting to enterprise SSID: "));
    Serial.println(ssid);

    WiFi.disconnect(true);   // turn radio off, reset state — required
    delay(100);
    WiFi.mode(WIFI_STA);
    delay(100);

    // No CA cert, no client cert — guest-class PEAP/MSCHAPv2
    WiFi.begin(ssid, WPA2_AUTH_PEAP, identity, user, pass);

    uint32_t start = millis();
    int dots = 0;
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= timeout_ms) {
            Serial.println(F("\n[WiFi] connection timeout"));
            return false;
        }
        delay(250);
        if (++dots % 8 == 0) Serial.print('.');
    }

    Serial.println(F("\n[WiFi] connected"));
    Serial.print(F("[WiFi] IP: "));
    Serial.println(WiFi.localIP());
    Serial.print(F("[WiFi] RSSI: "));
    Serial.print(WiFi.RSSI());
    Serial.println(F(" dBm"));
    return true;
}
```

Header to match (`lib/wifi_enterprise/wifi_enterprise.h`):

```cpp
#ifndef WIFI_ENTERPRISE_H
#define WIFI_ENTERPRISE_H
#include <stdint.h>

bool wifi_enterprise_connect(
    const char* ssid,
    const char* identity,
    const char* user,
    const char* pass,
    uint32_t    timeout_ms = 15000
);

#endif
```

---

## 3. Credential template

`include/wifi_credentials.h.example` (committed) → user copies to
`include/wifi_credentials.h` (gitignored):

```cpp
// WPA2-Enterprise PEAP/MSCHAPv2 credentials.
// Copy this file to wifi_credentials.h and fill in. wifi_credentials.h is
// gitignored so secrets don't ship.

#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H

#define WIFI_ENT_SSID                  "MSC guest"
#define WIFI_ENT_IDENTITY              "FILL_ME_IN"   // EAP identity (often user or user@domain)
#define WIFI_ENT_USERNAME              "FILL_ME_IN"   // MSCHAPv2 username (usually same as IDENTITY)
#define WIFI_ENT_PASSWORD              "FILL_ME_IN"   // MSCHAPv2 password

#define WIFI_ENT_CONNECT_TIMEOUT_MS    15000          // 15 s initial connect
#define WIFI_ENT_RECONNECT_INTERVAL_MS 5000           // 5 s background retry

#endif
```

Detection guard in `main.cpp`:

```cpp
if (strcmp(WIFI_ENT_USERNAME, "FILL_ME_IN") == 0) {
    Serial.println(F("[WiFi] credentials not set; skipping enterprise connect"));
} else {
    wifi_enterprise_connect(
        WIFI_ENT_SSID, WIFI_ENT_IDENTITY,
        WIFI_ENT_USERNAME, WIFI_ENT_PASSWORD,
        WIFI_ENT_CONNECT_TIMEOUT_MS);
}
```

---

## 4. Failure modes seen in the demos

| Source | On timeout | Reconnect | Notes |
|--------|------------|-----------|-------|
| GravityProbe (both) | `ESP.restart()` after 30 s | hard reboot loop | brutal but works on flaky setups |
| floppi flight_controller | log + continue without WiFi | `WiFi.reconnect()` every 5 s in loop | production pattern |
| PIO canonical example | block in connect loop | none | demo only |

**Recommended for esp32_field_node**: floppi pattern — initial 15 s connect
timeout, periodic `WiFi.reconnect()` in `loop()`, never reboot, never block.

```cpp
// in loop():
if (WiFi.status() != WL_CONNECTED) {
    static uint32_t last_retry = 0;
    if (millis() - last_retry >= WIFI_ENT_RECONNECT_INTERVAL_MS) {
        last_retry = millis();
        WiFi.reconnect();
    }
}
```

---

## 5. Quirks worth knowing

1. **`WiFi.disconnect(true)` before `WiFi.begin()` is mandatory.** The
   `true` turns off the radio. Skipping this causes silent failures from
   stale WPA state. Both GravityProbe demos and floppi all do it.

2. **Identity vs. username can differ.** Most guest deployments accept
   identical values; eduroam-style "user@homeorg.tld" identities differ
   from the username. We expose both as separate `#define`s.

3. **No CA cert = no PEAP server verification.** Acceptable for guest
   networks (MSC guest is the explicit target). Not for production
   eduroam-style auth.

4. **PEAP/MSCHAPv2 is implicit.** The Arduino-ESP32 API has a single
   `WPA2_AUTH_PEAP` enum. There is no explicit "PEAP/MSCHAPv2" — passing
   identity + user + pass to `WiFi.begin()` triggers MSCHAPv2 as the inner
   method automatically.

5. **`WiFi.mode(WIFI_STA)` is required.** Without it, `WiFi.begin()` may
   silently no-op.

6. **Polling cadence matters.** 250 ms is the floppi value and works.
   Faster (e.g. 50 ms) can race the WiFi subsystem. Slower (1000 ms+)
   adds startup latency.

7. **Auto-reconnect can be flaky.** `WiFi.setAutoReconnect(true)` exists
   but isn't reliable across all ESP32 silicon revisions. Manual
   `WiFi.reconnect()` in loop() is the gold standard.

---

## 6. References cited (full paths)

- `/home/devel/GravityProbe/esp32_ewpa2_iic_091/esp32_ewpa2_iic_091.ino`
- `/home/devel/GravityProbe/esp32_ent_wpa2_peap_web80/esp32_ent_wpa2_peap_web80.ino`
- `/home/devel/floppi/flight_controller/include/wifi_credentials.h`
- `/home/devel/floppi/flight_controller/src/wifi_manager.cpp`
- `/home/devel/.platformio/packages/framework-arduinoespressif32/libraries/WiFi/examples/WiFiClientEnterprise/WiFiClientEnterprise.ino`
- `/home/devel/.platformio/packages/framework-arduinoespressif32/package.json`
  (Arduino-ESP32 v3.20017.241212)
- `/home/devel/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32/include/wpa_supplicant/esp_supplicant/include/esp_wpa2.h`
