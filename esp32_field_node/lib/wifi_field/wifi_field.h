// WiFi connection helpers for esp32_field_node.
//
// Three primitives:
//   wifi_field_connect_open       — open network, no password
//   wifi_field_connect_enterprise — WPA2-Enterprise PEAP/MSCHAPv2 (no certs)
//   wifi_field_probe_portal       — HTTP probe for captive-portal status

#ifndef ESP32_FIELD_NODE_WIFI_FIELD_H
#define ESP32_FIELD_NODE_WIFI_FIELD_H

#include <stdint.h>

bool wifi_field_connect_open(const char* ssid, uint32_t timeout_ms = 15000);

bool wifi_field_connect_enterprise(const char* ssid,
                                   const char* identity,
                                   const char* user,
                                   const char* pass,
                                   uint32_t    timeout_ms = 15000);

// Returns one of: "online", "captive", "noprobe", "error".
//   online  = direct internet (HTTP 204 from gstatic)
//   captive = behind a portal (HTTP 200 or 30x redirect)
//   noprobe = WiFi not connected
//   error   = transport-level failure
const char* wifi_field_probe_portal();

void wifi_field_disconnect();

#endif
