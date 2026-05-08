# M3 OPEN-path connection — verified end-to-end

> Captured: 2026-05-08
> Source: live capture from `/dev/ttyUSB0` after M3 firmware upload
> Raw log: `tests/results/m3-connect-2026-05-08.txt` (gitignored)

## Result: full success

The M3 firmware booted, scanned WiFi, auto-connected to `MSC - GUEST`
as an OPEN network, got an IP via DHCP, ran the captive-portal probe,
and rendered SSID + IP + MAC + RSSI + portal status to the OLED — all
in one boot cycle, fully autonomous.

## Verbatim log

```
[wifi] auto: no enterprise creds; trying open 'MSC - GUEST'
[wifi] connecting open: 'MSC - GUEST'
[wifi] connected
[wifi] portal probe HTTP 302
[wifi] mode=OPEN ssid='MSC - GUEST' ip=10.232.20.126 mac=0C:B8:15:C1:39:B8 rssi=-79dBm portal=captive
```

## What this confirms

| Claim | Evidence |
|-------|----------|
| `MSC - GUEST` is OPEN | `WiFi.begin(ssid)` succeeded with no password |
| It's behind a captive portal | HTTP probe returned 302 (redirect to portal URL) |
| ESP32 STA MAC is `0C:B8:15:C1:39:B8` | Reported pre-connect (always available) |
| DHCP works on this network | Got `10.232.20.126/x` without further action |
| `wifi_field_probe_portal()` correctly detects portals | HTTP 302 → "captive" string |
| User's M3 ask works | SSID, IP, MAC, RSSI, portal all rendered to OLED via `display_connected()` |
| Auto-refresh on 10 s cadence works | Three portal probes seen in capture: T+~0, T+10, T+20 |
| Heartbeat does not block on connection | 6 heartbeats over a 33 s capture |

## Implications

The MSC - GUEST captive portal blocks general internet but does NOT
block the WiFi association itself. The ESP32 has a working IP on the
local LAN at `10.232.20.0/x`.

What the node CANNOT do without manual portal authentication:
- Reach external HTTP/HTTPS servers
- NTP, MQTT, etc.

What the node CAN do today:
- Communicate with other devices on the same `10.232.20.0/x` subnet
- Run an mDNS responder visible to local LAN clients (future enhancement)
- Auto-detect when the portal has been authenticated externally — the
  10-second probe will flip from "captive" → "online" once a laptop
  on the same SSID has signed into the portal

## Out of scope (future, deliberately deferred)

Auto-completing the captive-portal form is non-trivial:
1. Parse the HTTP 302 `Location` header
2. Follow to the portal URL, fetch the form HTML
3. POST credentials with the correct anti-CSRF token
4. Vendor-specific form structure varies

The user can authenticate from a laptop on the same SSID and the
ESP32's probe will pick up the change at the next refresh.

## What's NOT yet verified

The M3 **enterprise path** (`UAA WiFi -MatSu`) is built but not
live-tested — we don't have UAA credentials in this session. The code
is structurally identical to the canonical PIO `WiFiClientEnterprise`
example and the working `~/GravityProbe/esp32_ewpa2_iic_091/` demo.

To test it:
```bash
cp include/wifi_credentials.local.h.example include/wifi_credentials.local.h
# edit wifi_credentials.local.h: set WIFI_ENT_USERNAME / WIFI_ENT_PASSWORD
pio run -t upload --upload-port /dev/ttyUSB0
```

On the next boot, `connectAuto()` sees the non-placeholder username and
takes the enterprise path. Expected output:

```
[wifi] auto: enterprise creds present; trying enterprise
[wifi] connecting enterprise (PEAP/MSCHAPv2 no certs): 'UAA WiFi -MatSu'
[wifi] connected
[wifi] mode=WPA2-Ent ssid='UAA WiFi -MatSu' ip=... mac=... rssi=...dBm portal=...
```

## Hardware identity captured

For reference / NVS / future device-fleet management:

```
ESP32-D0WDQ6 rev 1, 2 cores @ 240 MHz, 4 MB flash
STA MAC: 0C:B8:15:C1:39:B8
```
