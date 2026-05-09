#include "wifi_field.h"

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_wpa2.h"

static bool wait_connected(uint32_t timeout_ms) {
  uint32_t start = millis();
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start >= timeout_ms) {
      Serial.println("\n[wifi] connection timeout");
      return false;
    }
    delay(250);
    if (++dots % 8 == 0) Serial.print('.');
  }
  Serial.println("\n[wifi] connected");
  return true;
}

bool wifi_field_connect_open(const char* ssid, uint32_t timeout_ms) {
  if (!ssid) return false;
  Serial.printf("[wifi] connecting open: '%s'\n", ssid);

  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);

  WiFi.begin(ssid);
  return wait_connected(timeout_ms);
}

bool wifi_field_connect_psk(const char* ssid,
                            const char* password,
                            uint32_t    timeout_ms) {
  if (!ssid || !password) return false;
  Serial.printf("[wifi] connecting WPA2-PSK: '%s'\n", ssid);

  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);

  WiFi.begin(ssid, password);
  return wait_connected(timeout_ms);
}

bool wifi_field_connect_enterprise(const char* ssid,
                                   const char* identity,
                                   const char* user,
                                   const char* pass,
                                   uint32_t    timeout_ms) {
  if (!ssid || !identity || !user || !pass) {
    Serial.println("[wifi] enterprise: null credential");
    return false;
  }
  Serial.printf("[wifi] connecting enterprise (PEAP/MSCHAPv2 no certs): '%s'\n", ssid);

  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);

  // No CA cert, no client cert — guest-class PEAP/MSCHAPv2.
  WiFi.begin(ssid, WPA2_AUTH_PEAP, identity, user, pass);
  return wait_connected(timeout_ms);
}

const char* wifi_field_probe_portal() {
  if (WiFi.status() != WL_CONNECTED) return "noprobe";

  HTTPClient http;
  // Google's connectivity check — same endpoint Android uses. HTTP/204 if
  // direct internet; HTTP/200 with content (or 30x) if behind a portal.
  if (!http.begin("http://connectivitycheck.gstatic.com/generate_204")) {
    return "error";
  }
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.setTimeout(5000);

  int rc = http.GET();
  http.end();

  Serial.printf("[wifi] portal probe HTTP %d\n", rc);
  if (rc == 204) return "online";
  if (rc < 0)    return "error";
  return "captive";  // 200, 30x, or anything else = portal
}

void wifi_field_disconnect() {
  WiFi.disconnect(true);
  delay(100);
}
