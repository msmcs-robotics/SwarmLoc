# MSC - GUEST network characterization — empirical reframe

> Captured: 2026-05-08
> Source: First M1 scan on the actual hardware, on-site near MSC
> Supersedes (partially): `msc-guest-wifi-no-certs.md`

## The reframe

The M1 scanner reports two `MSC - GUEST` BSSIDs (channels 6 and 11) and
**both report `WIFI_AUTH_OPEN`**. That contradicts the working assumption
we carried since project bootstrap, which was "WPA2-Enterprise without
certs."

Most likely interpretation:

> `MSC - GUEST` is an **open WiFi** that uses a **captive portal** (HTTP
> redirect) for guest authentication. The username/password the user
> remembers entering is consumed by a web form on the portal page,
> *not* by the WPA2-Enterprise EAP exchange.

This is the most common guest-network architecture today — open SSID,
captive portal handles auth at the HTTP layer.

## Implications for M3

| Path | Trigger | Status |
|------|---------|--------|
| **Open + captive portal** | `WIFI_ENT_SSID == "MSC - GUEST"` + `auth = OPEN` from M1 | **Primary M3 path** |
| WPA2-Enterprise PEAP no-cert | If we discover an enterprise SSID we want | Still useful — keep `wifi_enterprise_connect()` available |
| WPA2-PSK | Not relevant to MSC, but trivial via `WiFi.begin(ssid, pass)` | Cheap to support |

`wifi_enterprise_connect()` from
`findings/peap-mschapv2-reference.md` is **still valid code** — it just
isn't the path for `MSC - GUEST`. We retain it because:

1. The same hardware may need to connect to `UAA WiFi -MatSu` (genuinely
   WPA2-Enterprise) for testing
2. Other deployment sites may have real enterprise networks
3. The user will recognize the demo when they see it

## Captive-portal detection — design

A standard probe used by Android, iOS, and Linux NetworkManager:

```
GET http://connectivitycheck.gstatic.com/generate_204
```

Behavior:
- **Direct internet**: HTTP/204 No Content — we're online, no portal
- **Captive portal**: HTTP/200 + HTML body, OR a 30x redirect to the
  portal URL — we're behind a portal

Alternative endpoints commonly used:
- `http://detectportal.firefox.com/canonical.html` — expects body `success`
- `http://www.msftconnecttest.com/connecttest.txt` — expects body `Microsoft Connect Test`
- `http://neverssl.com` — always plain HTTP, never redirected by spec

Recommend `connectivitycheck.gstatic.com/generate_204` (Google's, also
used by Android). Cleanest signal.

## M3 connection flow (revised)

```
1. WiFi.mode(WIFI_STA); WiFi.disconnect(true); delay(100)
2. WiFi.begin("MSC - GUEST")                  // OPEN — no key
3. wait for WL_CONNECTED, timeout 15s
4. probe http://connectivitycheck.gstatic.com/generate_204
5. if 204:    "online — no portal"
   if 200 or 30x to portal_url:
       "captive portal at <url> — open in browser to authenticate"
       (we do NOT auto-fill the form for the user; that's a separate scope decision)
6. display SSID, IP, RSSI, portal status on OLED
```

## What this does NOT change

- The `wifi_enterprise_connect()` snippet stays valid for any future
  WPA2-Enterprise SSID
- The `wifi_credentials.h.example` template stays committed; it now
  documents BOTH paths
- The M1 scanner is the source of truth — when in doubt, run a scan

## Open follow-ups

- [ ] (M3) Implement open-network connect path
- [ ] (M3) Implement captive-portal probe with the gstatic URL
- [ ] (M3) Display: render "online" vs "portal at <url>" on OLED
- [ ] (M3) Decide later if we want to auto-submit the portal form. Out of
  scope for this session — that's a "scope check with user" topic.

## User confirmation (2026-05-08, post-scan)

> "ahh ok yes the UAA wifi matsu was the WPA2 enterprise"
> — user

So the network the user previously connected to with PEAP/MSCHAPv2 (no
certs) is **`UAA WiFi -MatSu`**, not `MSC - GUEST`. Both are visible from
the dev location. This means:

- `MSC - GUEST` — OPEN, captive portal expected → **M3 path 1**
- `UAA WiFi -MatSu` — WPA2-ENTERPRISE, PEAP no certs (per user) → **M3 path 2**

Both paths get implemented in M3. The user can choose which to connect
via the credentials file (`#define WIFI_MODE` selector or distinct
`#define`s for each network).
