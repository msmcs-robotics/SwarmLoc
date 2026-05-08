# 2026-05-08 — Execution plan: M0 → M3 in one session

> Created: 2026-05-08, autonomous-execution session
> Trigger: User wired up the SSD1306 OLED, asked to "do everything now",
>          authorized parallel agents.

## Goal

Drive `esp32_field_node` from "M0 stub built, hardware wired but not yet
flashed" to:

- **M0** verified on hardware (boot banner, I2C scan finds 0x3C)
- **M1** implemented + verified (WiFi scanner over Serial, encryption type
  string mapping)
- **M2** implemented + verified (U8g2 OLED rendering of boot banner +
  scan results)
- **M3** **template** built (compiles cleanly, awaits user credentials —
  cannot live-test without MSC guest username/password)
- Helper scripts (`scripts/upload_and_capture.sh` etc.)
- Session record + roadmap/todo updates + git commit

**Out of scope for this session (deferred):**
- M4 (MPU6050) — hardware not wired
- Live M3 connection test — credentials unavailable

## Starting state

- Files exist: `platformio.ini`, `src/main.cpp`, `docs/{scope,roadmap,todo,README}.md`
- Findings exist: `existing-demo-inventory.md`, `msc-guest-wifi-no-certs.md`
- Build verified: 26 s, 21.8% flash, 6.7% RAM
- Hardware: ESP32-WROOM-32D on `/dev/ttyUSB0`, SSD1306 0.96" wired (per user)
- Decisions locked: `olikraus/U8g2` lib, PEAP/MSCHAPv2 no-cert for M3

## Phases

### Phase 0 — Setup (now)

- [x] Write this plan
- [ ] Spawn **Agent A**: extract minimum-viable PEAP no-cert reference from
  `~/GravityProbe/esp32_ewpa2_iic_091/` + `~/floppi/flight_controller/`.
  Output: `findings/peap-mschapv2-reference.md`. Background.
- [ ] Spawn **Agent B**: extract U8g2 SSD1306 128×64 ESP32 init reference
  from `GPS_module/GPS_OLED_091/` + `DWS1000_UWB/include/display.h`.
  Output: `findings/u8g2-ssd1306-128x64-reference.md`. Background.

### Phase 1 — M0 hardware verification

- [ ] Confirm `/dev/ttyUSB0` is reachable (`ls -la`, no other process holding)
- [ ] `pio run -t upload --upload-port /dev/ttyUSB0 -d esp32_field_node`
- [ ] Capture serial 10 s via `stty + timeout cat` (DWS1000_UWB-style)
- [ ] Verify output contains: `=== esp32_field_node — bootstrap (M0) ===`,
  chip ID line, `device @ 0x3C  <- expected SSD1306`, `[heartbeat]`
- [ ] If 0x3C absent: STOP, report wiring issue, do not advance to M1
- [ ] Save raw output to `tests/results/m0-boot-2026-05-08.txt` (gitignored)
- [ ] Update `docs/todo.md` and `docs/roadmap.md` (M0 ✓)

### Phase 2 — M1 WiFi scanner

- [ ] Extend `src/main.cpp`:
  - `#include <WiFi.h>`
  - In `setup()`: WiFi mode STA + disconnect, kick off first scan
  - Add `wifiScanAndPrint()`: `WiFi.scanNetworks()` synchronous, then iterate
    and print SSID, RSSI (dBm), channel, encryption (mapped to string)
  - Encryption-int → string helper covering all `WIFI_AUTH_*` enum values
  - In `loop()`: read serial input; if line == "scan", trigger re-scan;
    auto-rescan every 30 s
- [ ] Build, upload, capture 25 s
- [ ] Save raw output to `tests/results/m1-scan-2026-05-08.txt`
- [ ] Document observed networks in `findings/wifi-scan-sample.md` (with
  emphasis on whether "MSC guest" was visible and what its `WIFI_AUTH_*`
  was)
- [ ] Update todo + roadmap (M1 ✓)

### Phase 3 — M2 OLED display

- [ ] Wait for Agent B (or fall back to direct file reads if it lags)
- [ ] Add `lib/display/display.{h,cpp}` (or inline in `src/`):
  - `U8G2_SSD1306_128X64_NONAME_F_HW_I2C` constructor with default I2C pins
  - `display_init()`, `display_banner(line1, line2)`, `display_wifi_list(results)`
- [ ] Update `main.cpp` to call display helpers after I2C scan and after
  each WiFi scan
- [ ] Build, upload, capture
- [ ] Visual verify on hardware (heartbeat on serial confirms program is
  running; user will verify the OLED visually)
- [ ] Save raw output to `tests/results/m2-display-2026-05-08.txt`
- [ ] Update todo + roadmap (M2 ✓)

### Phase 4 — M3 WPA2-Enterprise PEAP template (build-only)

- [ ] Wait for Agent A
- [ ] Create `include/wifi_credentials.h` with `#define` placeholders for
  SSID, IDENTITY, USERNAME, PASSWORD; mark file as gitignored or with a
  big "fill me in" comment
- [ ] Add `lib/wifi_enterprise/` (or inline) with:
  - PEAP no-cert connect using `esp_wpa2.h` (or `esp_eap_client.h` per
    Agent A's findings)
  - Auto-reconnect loop with timeout
  - Status: print SSID + IP + RSSI to serial AND OLED on connect
- [ ] Guard the connect call: only attempt if SSID/USER/PASS are not
  the placeholder values
- [ ] Build only — confirm exit 0
- [ ] Document required user actions in `findings/m3-credentials-howto.md`
- [ ] Update todo + roadmap (M3 ☑ template only — live test pending creds)

### Phase 5 — Scripts

- [ ] `scripts/upload_and_capture.sh PORT DURATION` — pio upload + stty + cat
- [ ] `scripts/build.sh` — `pio run -d esp32_field_node`
- [ ] `scripts/monitor.sh PORT [DURATION]` — capture serial only
- [ ] Make executable

### Phase 6 — Wrap-up

- [ ] Save session record `docs/archive/session-2026-05-08-m0-to-m3-template.md`
  with: what was done, what worked, what didn't, hardware state, next steps
- [ ] Update `docs/roadmap.md` (check off completed milestones)
- [ ] Update `docs/todo.md` (move completed → Recently Completed; populate Up Next)
- [ ] Update auto-memory `MEMORY.md` if anything significant
  (probably: "MSC guest is WPA2-Enterprise no-cert" goes here)
- [ ] `git add` + `git commit` with descriptive message

## Risks + mitigations

| Risk | Mitigation |
|------|------------|
| `/dev/ttyUSB0` busy / wrong perms | `ls -la` first, use `fuser` if needed, fall back to user error message |
| I2C scan doesn't see 0x3C | Stop at M0, report wiring; do NOT advance |
| ESP32 boot ROM noise corrupts the banner capture | Use 115200, raw stty, 10 s window — same pattern as DWS1000_UWB |
| `WiFi.scanNetworks()` returns 0 because user is far from any AP | Just report "0 networks" — that's still a working scanner |
| MSC guest not in scan range | Expected if not on-site — document and move on |
| M3 firmware compiles but live-test impossible | This is the explicit out-of-scope. Document handoff. |
| ESP-IDF API headers differ across Arduino-ESP32 versions | Agent A reports the exact header used by working demos |

## How to recover if I get stuck

- I2C scan empty → check user actually wired SDA=21, SCL=22 (DevKit labels
  may have moved). Ask before changing pins.
- Compile error from U8g2 — fall back to U8x8 text mode (smaller surface)
- Compile error from `esp_wpa2.h` — switch to `esp_eap_client.h` (newer core)

---

## Execution log (filled in as work progresses)

- 2026-05-08 — plan written, Agents A+B spawned, M0 upload pending
