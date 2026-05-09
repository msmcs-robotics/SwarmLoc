// WiFi credentials for esp32_field_node.
//
// This file is COMMITTED with placeholder defaults so the project builds
// out of the box. To set real credentials, copy
// `wifi_credentials.local.h.example` to `wifi_credentials.local.h` and
// edit it. The .local.h file is gitignored and will override the
// placeholders below via __has_include.
//
// Two named networks are pre-configured per the M1 scan
// (see docs/findings/wifi-scan-sample.md):
//   WIFI_OPEN_SSID — open network with captive portal (e.g. "MSC - GUEST")
//   WIFI_ENT_SSID  — WPA2-Enterprise PEAP/MSCHAPv2  (e.g. "UAA WiFi -MatSu")
//
// At boot, the firmware picks based on credential state:
//   - if WIFI_ENT_USERNAME is not "FILL_ME_IN" → try enterprise
//   - else                                    → try open

#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H

// Local override: per-developer secrets live in wifi_credentials.local.h
// (gitignored). If present, it can #define any of the symbols below and
// the placeholder defaults are skipped.
#if defined(__has_include)
#  if __has_include("wifi_credentials.local.h")
#    include "wifi_credentials.local.h"
#  endif
#endif

// === WPA2-PSK (regular pre-shared-key networks: home WiFi, phone hotspots) ===
// If WIFI_PSK_SSID and WIFI_PSK_PASSWORD are both non-placeholder, this
// is preferred at boot over open and enterprise paths.
#ifndef WIFI_PSK_SSID
#  define WIFI_PSK_SSID "FILL_ME_IN"
#endif
#ifndef WIFI_PSK_PASSWORD
#  define WIFI_PSK_PASSWORD "FILL_ME_IN"
#endif

// === Open / captive-portal network (M3 path 1) ===
#ifndef WIFI_OPEN_SSID
#  define WIFI_OPEN_SSID "MSC - GUEST"
#endif

// === WPA2-Enterprise PEAP / MSCHAPv2 (M3 path 2) ===
#ifndef WIFI_ENT_SSID
#  define WIFI_ENT_SSID "UAA WiFi -MatSu"
#endif
#ifndef WIFI_ENT_IDENTITY
#  define WIFI_ENT_IDENTITY "FILL_ME_IN"
#endif
#ifndef WIFI_ENT_USERNAME
#  define WIFI_ENT_USERNAME "FILL_ME_IN"
#endif
#ifndef WIFI_ENT_PASSWORD
#  define WIFI_ENT_PASSWORD "FILL_ME_IN"
#endif

// === Behavior ===
#ifndef WIFI_AUTO_CONNECT_ON_BOOT
#  define WIFI_AUTO_CONNECT_ON_BOOT 1   // 0 disables auto-connect
#endif
#ifndef WIFI_CONNECT_TIMEOUT_MS
#  define WIFI_CONNECT_TIMEOUT_MS 15000UL
#endif
#ifndef WIFI_RECONNECT_INTERVAL_MS
#  define WIFI_RECONNECT_INTERVAL_MS 5000UL
#endif

#endif // WIFI_CREDENTIALS_H
