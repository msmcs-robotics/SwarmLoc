# MSC guest WiFi — Enterprise without certs

> Recorded: 2026-05-08
> Source: User clarification during M0 bootstrap

## What we know

> "we shouldn't need certs to connect to the MSC guest wifi even though it
> is enterprise"
> — user, 2026-05-08

The MSC guest network appears to be **WPA2-Enterprise** in the encryption
sense but is configured to accept **username + password authentication
without client-side certificate validation**. This is the typical
deployment for guest-class campus / corporate enterprise networks.

## What that implies for the firmware

- **Auth method**: PEAP (Protected EAP) with **MSCHAPv2** as the inner method
- **Client cert**: not required (skip the EAP-TLS path entirely)
- **CA cert verification**: disabled — accept whatever cert the RADIUS server
  presents. The canonical
  `~/.platformio/packages/framework-arduinoespressif32/libraries/WiFi/examples/WiFiClientEnterprise/`
  does exactly this by default.
- **Credentials needed at runtime**: SSID, identity (username), password.
  No anonymous identity required for typical PEAP guest deployments.

## ESP32 API surface (Arduino framework)

The Arduino-ESP32 core wraps the IDF EAP API. The header / function names
depend on the core version installed:

```cpp
// Older core (<= 2.0.x): include esp_wpa2.h
#include "esp_wpa2.h"
esp_wifi_sta_wpa2_ent_set_identity((uint8_t*)EAP_IDENTITY, strlen(EAP_IDENTITY));
esp_wifi_sta_wpa2_ent_set_username((uint8_t*)EAP_USERNAME, strlen(EAP_USERNAME));
esp_wifi_sta_wpa2_ent_set_password((uint8_t*)EAP_PASSWORD, strlen(EAP_PASSWORD));
esp_wifi_sta_wpa2_ent_enable();
WiFi.begin(SSID);

// Newer core (3.x with IDF 5.x): use esp_eap_client.h
// esp_eap_client_set_identity(...), esp_eap_client_set_username(...), etc.
```

We need to **check which core our `platform = espressif32` install ships**
before writing M3 code. The reference examples in
`existing-demo-inventory.md` will resolve that — whichever header they use
is what's available.

## Open follow-ups

- [ ] (M1) Use the WiFi scanner to **confirm** the SSID exists and reports
  encryption type `WIFI_AUTH_WPA2_ENTERPRISE` (or a WPA3 variant).
- [ ] (M1) Probe MSC guest from a laptop on the same network: does the
  AP advertise PMF (802.11w)? Some guest enterprise SSIDs require it
  on the client side; ESP32 supports it but may need a flag.
- [ ] (M3) Decide whether to hardcode credentials in firmware (fast,
  insecure) or read them from flash / NVS at boot (slower, safer).
  For a development node, hardcoded is fine.

## What we explicitly do NOT need to do

- Do **not** chase EAP-TLS / client certificates / CA cert pinning.
- Do **not** burn time on captive-portal logic for MSC guest unless the
  M1 scanner reveals it's actually open-with-portal (unlikely given the
  user's statement).
- Do **not** treat this as if it were `eduroam` (which often does require
  cert validation, depending on the home institution's policy).
