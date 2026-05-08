// esp32_field_node — M3 (scanner + OLED + connect + portal probe)
//
// Layered on top of M2:
//   - M0: boot banner, chip info, I2C bus scan
//   - M1: WiFi scan + serial 'scan' command
//   - M2: SSD1306 OLED — boot banner + scan-results render
//   - M3: connect path (open + WPA2-Enterprise PEAP); HTTP captive-portal
//         probe; render SSID + IP + MAC + RSSI on display per user request

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include "display.h"
#include "wifi_field.h"
#include "wifi_credentials.h"

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static const uint32_t WIFI_RESCAN_MS = 30000UL;
static bool           g_display_ok    = false;
static bool           g_connected     = false;
static String         g_mode_label;
static String         g_connected_ssid;

// ---------------------------------------------------------------------------
// I2C bus scan (M0)
// ---------------------------------------------------------------------------
static bool scanI2C() {
  Serial.println("[i2c] scanning bus...");
  uint8_t found = 0;
  bool oled = false;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[i2c]   device @ 0x%02X%s\n", addr,
                    addr == OLED_I2C_ADDR ? "  <- expected SSD1306" : "");
      if (addr == OLED_I2C_ADDR) oled = true;
      found++;
    }
  }
  Serial.printf("[i2c] scan done, %u device(s) found\n", found);
  return oled;
}

// ---------------------------------------------------------------------------
// auth-mode → string (M1)
// ---------------------------------------------------------------------------
static const char* authToString(wifi_auth_mode_t a) {
  switch (a) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2-PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3-PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3-PSK";
    case WIFI_AUTH_WAPI_PSK:        return "WAPI-PSK";
    default:                        return "UNKNOWN";
  }
}

// ---------------------------------------------------------------------------
// Connected info → display + serial (the M3 ask: SSID, IP, MAC, RSSI)
// ---------------------------------------------------------------------------
static void renderConnected(const char* mode_label, const char* ssid) {
  String ip  = WiFi.localIP().toString();
  String mac = WiFi.macAddress();
  int    rssi = WiFi.RSSI();

  const char* portal = wifi_field_probe_portal();

  Serial.printf("[wifi] mode=%s ssid='%s' ip=%s mac=%s rssi=%ddBm portal=%s\n",
                mode_label, ssid, ip.c_str(), mac.c_str(), rssi, portal);

  if (g_display_ok) {
    display_connected(mode_label, ssid, ip.c_str(), mac.c_str(), rssi, portal);
  }
}

// ---------------------------------------------------------------------------
// WiFi scan (M1, kept; updated to render to display in M2)
// ---------------------------------------------------------------------------
static void wifiScanAndPrint() {
  if (g_display_ok && !g_connected) display_status("WiFi: scanning...");

  Serial.println("[wifi] scan starting (sync, including hidden)...");
  uint32_t t0 = millis();
  int n = WiFi.scanNetworks(/*async*/false, /*show_hidden*/true);
  uint32_t dt = millis() - t0;

  if (n < 0) {
    Serial.printf("[wifi] scan failed (rc=%d)\n", n);
    if (g_display_ok && !g_connected) display_status("WiFi: scan failed");
    return;
  }
  Serial.printf("[wifi] %d network(s) in %lums\n", n, (unsigned long)dt);

  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    bool is_target = ssid.equals(WIFI_OPEN_SSID) || ssid.equals(WIFI_ENT_SSID);
    Serial.printf("  %2d: %-32s rssi=%4d ch=%2d auth=%-16s%s\n",
                  i + 1,
                  ssid.length() ? ssid.c_str() : "<hidden>",
                  WiFi.RSSI(i),
                  WiFi.channel(i),
                  authToString(WiFi.encryptionType(i)),
                  is_target ? "  <-- target" : "");
  }

  if (g_display_ok && !g_connected) {
    constexpr int TOPN = 5;
    int top_count = (n > TOPN) ? TOPN : n;
    String      top_str[TOPN];
    const char* top_ptr[TOPN];
    int         top_rssi[TOPN];
    for (int i = 0; i < top_count; i++) {
      String s = WiFi.SSID(i);
      if (s.length() == 0) s = "(hidden)";
      top_str[i]  = s;
      top_ptr[i]  = top_str[i].c_str();
      top_rssi[i] = WiFi.RSSI(i);
    }
    display_wifi_list(top_ptr, top_rssi, top_count);
  }

  WiFi.scanDelete();
}

// ---------------------------------------------------------------------------
// Connect dispatchers (M3)
// ---------------------------------------------------------------------------
static bool connectOpen(const char* ssid) {
  if (!ssid || !ssid[0]) return false;
  if (g_display_ok) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Connect: %.20s", ssid);
    display_status(buf);
  }
  bool ok = wifi_field_connect_open(ssid, WIFI_CONNECT_TIMEOUT_MS);
  g_connected = ok;
  if (ok) {
    g_mode_label     = "OPEN";
    g_connected_ssid = ssid;
    renderConnected("OPEN", ssid);
  } else {
    if (g_display_ok) display_status("Connect failed (open)");
  }
  return ok;
}

static bool connectEnterprise() {
  if (g_display_ok) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Connect: %.20s", WIFI_ENT_SSID);
    display_status(buf);
  }
  bool ok = wifi_field_connect_enterprise(
      WIFI_ENT_SSID, WIFI_ENT_IDENTITY, WIFI_ENT_USERNAME, WIFI_ENT_PASSWORD,
      WIFI_CONNECT_TIMEOUT_MS);
  g_connected = ok;
  if (ok) {
    g_mode_label     = "WPA2-Ent";
    g_connected_ssid = WIFI_ENT_SSID;
    renderConnected("WPA2-Ent", WIFI_ENT_SSID);
  } else {
    if (g_display_ok) display_status("Connect failed (ent)");
  }
  return ok;
}

static bool connectAuto() {
  bool have_ent = strcmp(WIFI_ENT_USERNAME, "FILL_ME_IN") != 0
               && strcmp(WIFI_ENT_PASSWORD, "FILL_ME_IN") != 0;
  if (have_ent) {
    Serial.println("[wifi] auto: enterprise creds present; trying enterprise");
    return connectEnterprise();
  }
  Serial.printf("[wifi] auto: no enterprise creds; trying open '%s'\n",
                WIFI_OPEN_SSID);
  return connectOpen(WIFI_OPEN_SSID);
}

// ---------------------------------------------------------------------------
// Tiny serial command parser (M3-extended)
// ---------------------------------------------------------------------------
static String inputBuffer;

static void runCmd(const String& cmd) {
  if (cmd.equalsIgnoreCase("scan")) {
    wifiScanAndPrint();
  } else if (cmd.startsWith("connect open ")) {
    String s = cmd.substring(13);
    s.trim();
    connectOpen(s.c_str());
  } else if (cmd.equalsIgnoreCase("connect ent") ||
             cmd.equalsIgnoreCase("connect enterprise")) {
    connectEnterprise();
  } else if (cmd.equalsIgnoreCase("connect")) {
    connectAuto();
  } else if (cmd.equalsIgnoreCase("disconnect")) {
    wifi_field_disconnect();
    g_connected = false;
    if (g_display_ok) display_status("Disconnected");
  } else if (cmd.equalsIgnoreCase("status")) {
    if (g_connected && WiFi.status() == WL_CONNECTED) {
      renderConnected(g_mode_label.c_str(), g_connected_ssid.c_str());
    } else {
      Serial.printf("[wifi] not connected (status=%d)\n", (int)WiFi.status());
    }
  } else if (cmd.equalsIgnoreCase("portal")) {
    Serial.printf("[wifi] portal probe: %s\n", wifi_field_probe_portal());
  } else if (cmd.equalsIgnoreCase("help") || cmd.equalsIgnoreCase("?")) {
    Serial.println("[cmd] commands:");
    Serial.println("       scan                     re-scan WiFi");
    Serial.println("       connect                  auto (ent if creds set, else open)");
    Serial.println("       connect ent              WPA2-Enterprise (uses wifi_credentials.h)");
    Serial.println("       connect open <SSID>      open network");
    Serial.println("       disconnect");
    Serial.println("       status                   show connection info");
    Serial.println("       portal                   re-probe captive portal");
    Serial.println("       help");
  } else {
    Serial.printf("[cmd] unknown: '%s' (try: help)\n", cmd.c_str());
  }
}

static void handleSerialInput() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      inputBuffer.trim();
      if (inputBuffer.length()) runCmd(inputBuffer);
      inputBuffer = "";
    } else if (inputBuffer.length() < 96) {
      inputBuffer += c;
    }
  }
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("=== esp32_field_node — M3 (WiFi + OLED + connect) ===");
  Serial.printf("CPU: %s rev %d, %u core(s) @ %lu MHz, flash %luKB\n",
                ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getChipCores(), (unsigned long)ESP.getCpuFreqMHz(),
                (unsigned long)(ESP.getFlashChipSize() / 1024));

  Wire.begin();
  bool oled_present = scanI2C();

  if (oled_present) {
    g_display_ok = display_init();
    if (g_display_ok) {
      Serial.println("[oled] init OK");
      display_banner("SwarmLoc node", "M3 boot");
    } else {
      Serial.println("[oled] init FAILED — running headless");
    }
  } else {
    Serial.println("[oled] not on I2C bus — running headless");
  }
  delay(1500);

  // STA MAC is stable based on chip — useful even pre-connect
  Serial.printf("[wifi] STA MAC: %s\n", WiFi.macAddress().c_str());

  Serial.println("[wifi] mode=STA; clearing prior association...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  wifiScanAndPrint();

#if WIFI_AUTO_CONNECT_ON_BOOT
  Serial.println("[wifi] auto-connect on boot — see wifi_credentials.h");
  connectAuto();
#endif

  Serial.println("[boot] entering loop() — type 'help' for commands");
}

void loop() {
  static uint32_t lastScan        = 0;
  static uint32_t lastBeat        = 0;
  static uint32_t lastRetry       = 0;
  static uint32_t lastInfoRefresh = 0;
  static uint32_t beat            = 0;

  handleSerialInput();

  uint32_t now = millis();

  // If we believe we should be connected but the radio dropped, retry.
  if (g_connected && WiFi.status() != WL_CONNECTED) {
    if (now - lastRetry >= WIFI_RECONNECT_INTERVAL_MS) {
      lastRetry = now;
      Serial.println("[wifi] disconnected — reconnecting...");
      WiFi.reconnect();
    }
  }

  // Periodic refresh of the connected screen — RSSI changes, portal can flip
  if (g_connected && WiFi.status() == WL_CONNECTED) {
    if (now - lastInfoRefresh >= 10000UL) {
      lastInfoRefresh = now;
      renderConnected(g_mode_label.c_str(), g_connected_ssid.c_str());
    }
  }

  // Auto re-scan only when not connected (scan-while-associated can hang)
  if (!g_connected && (now - lastScan >= WIFI_RESCAN_MS)) {
    lastScan = now;
    wifiScanAndPrint();
  }

  if (now - lastBeat >= 5000UL) {
    lastBeat = now;
    Serial.printf("[heartbeat] %lu — uptime %lus, status=%d\n",
                  (unsigned long)(++beat), (unsigned long)(now / 1000),
                  (int)WiFi.status());
  }
  delay(20);
}
