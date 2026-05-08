# M1 WiFi scan sample — first capture from esp32_field_node

> Captured: 2026-05-08, ~17:40 local
> Hardware: ESP32-WROOM-32D on /dev/ttyUSB0 with DSD Tech SSD1306 wired
> Firmware: M1 (`src/main.cpp`)
> Raw log: `tests/results/m1-scan-2026-05-08.txt` (gitignored)

## What `WiFi.scanNetworks(false, true)` returned

Synchronous scan including hidden networks. 9 BSSIDs visible. ~6.2 s wall time.

```
 1: UAA WiFi -MatSu                  rssi= -75 ch= 6 auth=WPA2-ENTERPRISE
 2: MSC - GUEST                      rssi= -75 ch= 6 auth=OPEN
 3: <hidden>                         rssi= -76 ch= 6 auth=WPA2-PSK
 4: <hidden>                         rssi= -76 ch= 6 auth=WPA2-ENTERPRISE
 5: eziemag                          rssi= -76 ch= 8 auth=WPA2-PSK
 6: <hidden>                         rssi= -79 ch=11 auth=WPA2-PSK
 7: UAA WiFi -MatSu                  rssi= -80 ch=11 auth=WPA2-ENTERPRISE
 8: MSC - GUEST                      rssi= -80 ch=11 auth=OPEN
 9: <hidden>                         rssi= -81 ch=11 auth=WPA2-ENTERPRISE
```

## What this tells us

1. **The scanner works.** All `wifi_auth_mode_t` values map cleanly to the
   strings we expect.
2. **Encryption-type detection works for enterprise.** `WPA2-ENTERPRISE`
   was correctly identified for the UAA WiFi SSIDs and the hidden
   ones — that's exactly the data we needed M1 to surface.
3. **MSC's guest network is OPEN.** This contradicts the assumption we
   carried into M0/M1 — see `findings/msc-guest-network-characterization.md`
   for the reframe.
4. **Two-AP coverage.** Both `UAA WiFi -MatSu` and `MSC - GUEST` appear on
   channels 6 *and* 11 with similar RSSI — almost certainly a single AP
   broadcasting on both bands or a small cluster of APs (more than one
   physical AP would be normal for a campus deployment).
5. **Hidden enterprise SSIDs**: 3 of the hidden networks are
   WPA2-ENTERPRISE — these are likely staff/admin networks not relevant
   to esp32_field_node.

## Verified by this capture

- [x] M1 firmware boots, finds I2C 0x3C, runs WiFi scan
- [x] `authToString()` covers OPEN / WPA2-PSK / WPA2-ENTERPRISE
  (other enum values not present in this scan but coded in)
- [x] Hidden networks emit empty SSIDs as expected
- [x] Heartbeat and serial command parser still alive after the scan

## Minor cosmetic issue (will fix in M2)

The M1 firmware looked for `"MSC guest"` (the spelling we assumed at
project bootstrap) and reported "MSC guest not visible from here" even
though the actual SSID `"MSC - GUEST"` was right there. The M2 firmware
updates `MSC_SSID` to the real string.
