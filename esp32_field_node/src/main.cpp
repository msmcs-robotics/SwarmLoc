// esp32_field_node — M4 (M3 + MPU6050 IMU + web server, dual-core)
//
// Architecture (per user request):
//   - Core 0: IMU sampling task @ 20 Hz, pushes ImuReading via web_set_imu()
//   - Core 1: Arduino loop — WiFi management, HTTP server (port 80),
//             OLED refresh, serial CLI
//
// All I2C transactions on the shared bus (OLED @ 0x3C, MPU6050 @ 0x68)
// are serialized through g_i2c_mutex. Wire is not thread-safe; the mutex
// is mandatory now that two cores can drive it.
//
// OLED keeps its 3-row SSID / MAC / IP layout (no IMU data on display —
// IMU is for the flight controller project / web monitor).

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "display.h"
#include "wifi_field.h"
#include "wifi_credentials.h"
#include "imu.h"
#include "web.h"

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static const uint32_t WIFI_RESCAN_MS = 30000UL;
static const uint32_t IMU_PERIOD_MS  = 50;       // 20 Hz IMU sampling
static const uint32_t IMU_LOG_MS     = 5000;     // serial log cadence

static bool   g_display_ok = false;
static bool   g_imu_present = false;
static bool   g_web_ok      = false;
static bool   g_connected   = false;
static String g_mode_label;
static String g_connected_ssid;

static SemaphoreHandle_t g_i2c_mutex = nullptr;

// RAII scope-guard for the shared I2C bus.
struct I2CLock {
  I2CLock()  { if (g_i2c_mutex) xSemaphoreTake(g_i2c_mutex, portMAX_DELAY); }
  ~I2CLock() { if (g_i2c_mutex) xSemaphoreGive(g_i2c_mutex); }
};

// ---------------------------------------------------------------------------
// I2C bus scan (M0)
// ---------------------------------------------------------------------------
static const char* i2cDeviceName(uint8_t addr) {
  switch (addr) {
    case 0x3C: return "SSD1306 OLED";
    case 0x3D: return "SSD1306 OLED (alt addr)";
    case 0x68: return "MPU6050 IMU";
    case 0x69: return "MPU6050 IMU (AD0=high)";
    case 0x76: return "BME280/BMP280";
    case 0x77: return "BME280/BMP280 (alt)";
    case 0x40: return "INA219 / Si7021";
    case 0x29: return "VL53L0X ToF";
    default:   return "";
  }
}

struct I2CScanResult { bool oled; bool imu; };

static I2CScanResult scanI2C() {
  I2CScanResult r{false, false};
  Serial.println("[i2c] scanning bus...");
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      const char* name = i2cDeviceName(addr);
      if (name[0]) {
        Serial.printf("[i2c]   device @ 0x%02X  -- %s\n", addr, name);
      } else {
        Serial.printf("[i2c]   device @ 0x%02X\n", addr);
      }
      if (addr == OLED_I2C_ADDR) r.oled = true;
      if (addr == 0x68)          r.imu  = true;
      found++;
    }
  }
  Serial.printf("[i2c] scan done, %u device(s) found\n", found);
  return r;
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
// Connected info → display + serial (3-row OLED: SSID / MAC / IP)
// ---------------------------------------------------------------------------
static void renderConnected(const char* mode_label, const char* ssid) {
  String ip   = WiFi.localIP().toString();
  String mac  = WiFi.macAddress();
  int    rssi = WiFi.RSSI();

  const char* portal = wifi_field_probe_portal();
  Serial.printf("[wifi] mode=%s ssid='%s' ip=%s mac=%s rssi=%ddBm portal=%s\n",
                mode_label, ssid, ip.c_str(), mac.c_str(), rssi, portal);

  if (g_display_ok) {
    I2CLock lock;
    display_connected(ssid, mac.c_str(), ip.c_str());
  }
}

// ---------------------------------------------------------------------------
// WiFi scan
// ---------------------------------------------------------------------------
static void wifiScanAndPrint() {
  if (g_display_ok && !g_connected) {
    I2CLock lock;
    display_status("WiFi: scanning...");
  }

  Serial.println("[wifi] scan starting (sync, including hidden)...");
  uint32_t t0 = millis();
  int n = WiFi.scanNetworks(/*async*/false, /*show_hidden*/true);
  uint32_t dt = millis() - t0;

  if (n < 0) {
    Serial.printf("[wifi] scan failed (rc=%d)\n", n);
    if (g_display_ok && !g_connected) {
      I2CLock lock;
      display_status("WiFi: scan failed");
    }
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
    I2CLock lock;
    display_wifi_list(top_ptr, top_rssi, top_count);
  }

  WiFi.scanDelete();
}

// ---------------------------------------------------------------------------
// Connect dispatchers
// ---------------------------------------------------------------------------
static bool connectOpen(const char* ssid) {
  if (!ssid || !ssid[0]) return false;
  if (g_display_ok) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Connect: %.20s", ssid);
    I2CLock lock;
    display_status(buf);
  }
  bool ok = wifi_field_connect_open(ssid, WIFI_CONNECT_TIMEOUT_MS);
  g_connected = ok;
  if (ok) {
    g_mode_label     = "OPEN";
    g_connected_ssid = ssid;
    renderConnected("OPEN", ssid);
  } else if (g_display_ok) {
    I2CLock lock;
    display_status("Connect failed (open)");
  }
  return ok;
}

static bool connectPsk(const char* ssid, const char* password) {
  if (!ssid || !ssid[0] || !password) return false;
  if (g_display_ok) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Connect: %.20s", ssid);
    I2CLock lock;
    display_status(buf);
  }
  bool ok = wifi_field_connect_psk(ssid, password, WIFI_CONNECT_TIMEOUT_MS);
  g_connected = ok;
  if (ok) {
    g_mode_label     = "WPA2-PSK";
    g_connected_ssid = ssid;
    renderConnected("WPA2-PSK", ssid);
  } else if (g_display_ok) {
    I2CLock lock;
    display_status("Connect failed (psk)");
  }
  return ok;
}

static bool connectEnterprise() {
  if (g_display_ok) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Connect: %.20s", WIFI_ENT_SSID);
    I2CLock lock;
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
  } else if (g_display_ok) {
    I2CLock lock;
    display_status("Connect failed (ent)");
  }
  return ok;
}

static bool connectAuto() {
  // Priority: PSK > Enterprise > Open. Matches user's typical preference —
  // PSK (e.g. an iPhone hotspot) is the most likely "I want this to work
  // right now" target, and most often actually has internet behind it.
  bool have_psk = strcmp(WIFI_PSK_SSID, "FILL_ME_IN") != 0
               && strcmp(WIFI_PSK_PASSWORD, "FILL_ME_IN") != 0;
  if (have_psk) {
    Serial.printf("[wifi] auto: PSK creds present; trying '%s'\n", WIFI_PSK_SSID);
    return connectPsk(WIFI_PSK_SSID, WIFI_PSK_PASSWORD);
  }

  bool have_ent = strcmp(WIFI_ENT_USERNAME, "FILL_ME_IN") != 0
               && strcmp(WIFI_ENT_PASSWORD, "FILL_ME_IN") != 0;
  if (have_ent) {
    Serial.println("[wifi] auto: enterprise creds present; trying enterprise");
    return connectEnterprise();
  }

  Serial.printf("[wifi] auto: no PSK / enterprise creds; trying open '%s'\n",
                WIFI_OPEN_SSID);
  return connectOpen(WIFI_OPEN_SSID);
}

// ---------------------------------------------------------------------------
// IMU task — Core 0 — 20 Hz read, publishes to web snapshot
// ---------------------------------------------------------------------------
static void imuTask(void* /*arg*/) {
  Serial.printf("[imu-task] running on core %d, prio %d\n",
                xPortGetCoreID(), (int)uxTaskPriorityGet(NULL));

  uint32_t last_log = 0;
  for (;;) {
    ImuReading r;
    bool ok = false;
    {
      I2CLock lock;
      ok = imu_read(&r);
    }
    if (ok) {
      web_set_imu(r);

      uint32_t now = millis();
      if (now - last_log >= IMU_LOG_MS) {
        last_log = now;
        Serial.printf(
            "[imu] ax=%+.2f ay=%+.2f az=%+.2f g | "
            "gx=%+.1f gy=%+.1f gz=%+.1f deg/s | t=%.1fC\n",
            r.accel_x, r.accel_y, r.accel_z,
            r.gyro_x,  r.gyro_y,  r.gyro_z,
            r.temp_c);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(IMU_PERIOD_MS));
  }
}

// ---------------------------------------------------------------------------
// Serial command parser
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
  } else if (cmd.equalsIgnoreCase("connect psk")) {
    connectPsk(WIFI_PSK_SSID, WIFI_PSK_PASSWORD);
  } else if (cmd.equalsIgnoreCase("connect")) {
    connectAuto();
  } else if (cmd.equalsIgnoreCase("disconnect")) {
    wifi_field_disconnect();
    g_connected = false;
    if (g_display_ok) {
      I2CLock lock;
      display_status("Disconnected");
    }
  } else if (cmd.equalsIgnoreCase("status")) {
    if (g_connected && WiFi.status() == WL_CONNECTED) {
      renderConnected(g_mode_label.c_str(), g_connected_ssid.c_str());
    } else {
      Serial.printf("[wifi] not connected (status=%d)\n", (int)WiFi.status());
    }
  } else if (cmd.equalsIgnoreCase("portal")) {
    Serial.printf("[wifi] portal probe: %s\n", wifi_field_probe_portal());
  } else if (cmd.equalsIgnoreCase("imu")) {
    if (!imu_ready()) {
      Serial.println("[imu] not initialized");
    } else {
      ImuReading r;
      bool ok;
      {
        I2CLock lock;
        ok = imu_read(&r);
      }
      if (ok) {
        Serial.printf("[imu] ax=%+.3f ay=%+.3f az=%+.3f g | "
                      "gx=%+.2f gy=%+.2f gz=%+.2f deg/s | t=%.2fC | t_ms=%lu\n",
                      r.accel_x, r.accel_y, r.accel_z,
                      r.gyro_x,  r.gyro_y,  r.gyro_z,
                      r.temp_c, (unsigned long)r.millis_when);
      } else {
        Serial.println("[imu] read failed");
      }
    }
  } else if (cmd.equalsIgnoreCase("web")) {
    if (g_web_ok && WiFi.status() == WL_CONNECTED) {
      Serial.printf("[web] http://%s/  (poll /imu for JSON)\n",
                    WiFi.localIP().toString().c_str());
    } else {
      Serial.println("[web] not running");
    }
  } else if (cmd.equalsIgnoreCase("help") || cmd.equalsIgnoreCase("?")) {
    Serial.println("[cmd] commands:");
    Serial.println("       scan                     re-scan WiFi");
    Serial.println("       connect                  auto (PSK > ent > open by cred presence)");
    Serial.println("       connect psk              WPA2-PSK using wifi_credentials.h values");
    Serial.println("       connect ent              WPA2-Enterprise using wifi_credentials.h values");
    Serial.println("       connect open <SSID>      open network");
    Serial.println("       disconnect");
    Serial.println("       status                   show connection info");
    Serial.println("       portal                   re-probe captive portal");
    Serial.println("       imu                      single MPU6050 reading");
    Serial.println("       web                      print web URL");
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
  Serial.println("=== esp32_field_node — M4 (WiFi + OLED + IMU + web) ===");
  Serial.printf("CPU: %s rev %d, %u core(s) @ %lu MHz, flash %luKB\n",
                ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getChipCores(), (unsigned long)ESP.getCpuFreqMHz(),
                (unsigned long)(ESP.getFlashChipSize() / 1024));
  Serial.printf("[boot] running on core %d\n", xPortGetCoreID());

  // I2C bus + mutex (no other task is running yet — setup is single-threaded)
  Wire.begin();
  g_i2c_mutex = xSemaphoreCreateMutex();

  I2CScanResult i2c = scanI2C();
  g_imu_present = i2c.imu;

  if (i2c.oled) {
    g_display_ok = display_init();
    if (g_display_ok) {
      Serial.println("[oled] init OK");
      display_banner("SwarmLoc node", "M4 boot");
    } else {
      Serial.println("[oled] init FAILED — running headless");
    }
  } else {
    Serial.println("[oled] not on I2C bus — running headless");
  }

  if (g_imu_present) {
    if (!imu_init()) g_imu_present = false;
  } else {
    Serial.println("[imu] no MPU6050 on bus — IMU task will not start");
  }

  delay(1500);

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

  // Web server: only meaningful once WiFi is up.
  if (WiFi.status() == WL_CONNECTED) {
    g_web_ok = web_init();
  }

  // IMU task — pinned to Core 0. Arduino loop stays on Core 1.
  if (imu_ready()) {
    BaseType_t rc = xTaskCreatePinnedToCore(
        imuTask, "imu_task",
        /*stack*/ 4096, /*arg*/ nullptr,
        /*prio*/ 2,     /*handle*/ nullptr,
        /*core*/ 0);
    Serial.printf("[boot] IMU task spawn rc=%d (Core 0)\n", (int)rc);
  }

  Serial.println("[boot] entering loop() — type 'help' for commands");
}

void loop() {
  static uint32_t lastScan        = 0;
  static uint32_t lastBeat        = 0;
  static uint32_t lastRetry       = 0;
  static uint32_t lastInfoRefresh = 0;
  static uint32_t beat            = 0;

  handleSerialInput();
  web_handle();   // pump HTTP requests on Core 1

  uint32_t now = millis();

  // Reconnect if we believed we were connected but the radio dropped.
  if (g_connected && WiFi.status() != WL_CONNECTED) {
    if (now - lastRetry >= WIFI_RECONNECT_INTERVAL_MS) {
      lastRetry = now;
      Serial.println("[wifi] disconnected — reconnecting...");
      WiFi.reconnect();
    }
  }

  // Periodic refresh of the connected screen
  if (g_connected && WiFi.status() == WL_CONNECTED) {
    if (now - lastInfoRefresh >= 10000UL) {
      lastInfoRefresh = now;
      renderConnected(g_mode_label.c_str(), g_connected_ssid.c_str());
    }
  }

  // Auto-rescan only when not connected (scan-while-associated can hang)
  if (!g_connected && (now - lastScan >= WIFI_RESCAN_MS)) {
    lastScan = now;
    wifiScanAndPrint();
  }

  if (now - lastBeat >= 5000UL) {
    lastBeat = now;
    Serial.printf("[heartbeat] %lu — uptime %lus, status=%d, web=%d\n",
                  (unsigned long)(++beat), (unsigned long)(now / 1000),
                  (int)WiFi.status(), (int)g_web_ok);
  }
  delay(10);  // shorter than before — let web_handle() run more often
}
