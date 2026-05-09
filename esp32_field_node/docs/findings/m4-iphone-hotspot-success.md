# M4 (PSK path) — iPhone hotspot connection verified

> Captured: 2026-05-08
> Source: live capture from `/dev/ttyUSB0` after the WPA2-PSK firmware
>         (added `wifi_field_connect_psk()` and bumped PSK above ENT/OPEN
>         in the auto-connect priority)
> Raw log: `tests/results/m4-iphone-psk-2026-05-08.txt` (gitignored)

## Result: full success — and we have actual internet now

```
[wifi] auto: PSK creds present; trying 'iPhone'
[wifi] connecting WPA2-PSK: 'iPhone'

[wifi] connected
[wifi] portal probe HTTP 204
[wifi] mode=WPA2-PSK ssid='iPhone' ip=172.20.10.13 mac=0C:B8:15:C1:39:B8 rssi=-44dBm portal=online
[web] listening on http://172.20.10.13/
```

Portal probe returned **HTTP 204** — direct internet, no captive portal.
The previous MSC - GUEST runs always returned 302 (`captive`). On the
iPhone hotspot the ESP32 is genuinely on the open internet via the
phone's cellular link.

## What this confirms

| Claim | Evidence |
|-------|----------|
| WPA2-PSK path works | Connected to `iPhone` hotspot using `WiFi.begin(ssid, pass)` |
| PSK takes priority over Enterprise/Open | `[wifi] auto: PSK creds present; trying 'iPhone'` fires before any open/ent logic |
| Apple hotspot subnet | Got `172.20.10.13/28` — standard Apple "Personal Hotspot" range |
| Strong signal | -44 to -48 dBm (the iPhone is right there) |
| Real internet, no portal | HTTP probe returned 204 instead of 302 |
| OLED still working | Same 3-row layout (now showing `iPhone` / MAC / 172.20.10.13) |
| IMU still streaming | Heartbeats interleaved with `[imu] ax=... ay=... az=...` lines |
| Web server reachable | A browser hit the server during capture: `_handleRequest(): request handler not found` is the favicon.ico request — handler now added to silence it |
| Dual-core still cooperating | No mutex deadlocks, no I2C errors over 28 s |

## Live IMU evidence

The capture also caught the user moving the breakout mid-run:

```
T+ 0s: ax=-0.07 ay=+0.04 az=+1.03  (chip flat, gravity on z)
T+ 5s: ax=-0.16 ay=-0.14 az=+0.92  gx=+9 gy=+57 gz=-74  ← active motion
T+10s: ax=-0.29 ay=+0.07 az=+0.98  ← chip now tilted ~17° on x axis
```

That's exactly the behavior you'd see if someone picked up the breakout
and re-set it down at an angle. IMU is responsive.

## What this means for browser access

**You can now reach the IMU monitor.** Any device on the iPhone hotspot
should be able to:

```
http://172.20.10.13/
```

The page renders dark-themed, polls `/imu` every 100 ms, displays accel
x/y/z, gyro x/y/z, temperature, and sample age. Tilt the breakout and the
numbers update in near-real-time.

The MSC - GUEST captive-portal isolation was indeed blocking client-to-
client traffic (consistent with most public guest networks). The hotspot
fixes it because hotspots typically allow clients to talk to each other.

## Credential handling

iPhone password lives in `include/wifi_credentials.local.h` (gitignored).
That file is now created on this dev machine but **will not be committed**.
The committed `wifi_credentials.local.h.example` shows the pattern:

```c
#define WIFI_PSK_SSID      "iPhone"
#define WIFI_PSK_PASSWORD  "yourpasswordhere"
```

Other developers / fresh checkouts won't see the password. To re-create
on a new machine: copy the `.example` to `.local.h` and fill in the
real values.

## Auto-connect priority (now)

```
1. PSK    (if WIFI_PSK_SSID and WIFI_PSK_PASSWORD are non-placeholder)
2. ENT    (if WIFI_ENT_USERNAME and WIFI_ENT_PASSWORD are non-placeholder)
3. OPEN   (fallback — uses WIFI_OPEN_SSID, currently "MSC - GUEST")
```

So with the iPhone creds set, the node prefers the hotspot. Removing the
`.local.h` would fall back to OPEN against MSC - GUEST again.

## Side wins

- Confirms the favicon-request handler issue (now fixed with a tiny
  `/favicon.ico → 204` handler).
- IMU task tolerates a WiFi mode change without dropping samples.
- The same firmware now supports OPEN, PSK, and Enterprise — three
  paths covered.
