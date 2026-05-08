#include "display.h"
#include <string.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

bool display_init() {
  if (!u8g2.begin()) return false;
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
  return true;
}

void display_clear() {
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

void display_banner(const char* line1, const char* line2) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  if (line1) u8g2.drawStr(0, 25, line1);
  if (line2) u8g2.drawStr(0, 40, line2);
  u8g2.sendBuffer();
}

void display_status(const char* status_line) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  if (status_line) u8g2.drawStr(0, 32, status_line);
  u8g2.sendBuffer();
}

void display_connected(const char* mode_label,
                       const char* ssid,
                       const char* ip_str,
                       const char* mac_str,
                       int         rssi_dbm,
                       const char* portal_status) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  // Header
  char header[24];
  snprintf(header, sizeof(header), "Connected: %s",
           mode_label ? mode_label : "?");
  u8g2.drawStr(0, 10, header);

  // SSID (truncated)
  char buf[32];
  snprintf(buf, sizeof(buf), "%.20s", ssid ? ssid : "?");
  u8g2.drawStr(0, 22, buf);

  // IP
  snprintf(buf, sizeof(buf), "IP:  %s", ip_str ? ip_str : "?");
  u8g2.drawStr(0, 33, buf);

  // MAC
  snprintf(buf, sizeof(buf), "MAC: %s", mac_str ? mac_str : "?");
  u8g2.drawStr(0, 44, buf);

  // RSSI + portal status combined
  if (portal_status && portal_status[0]) {
    snprintf(buf, sizeof(buf), "%d dBm  %s", rssi_dbm, portal_status);
  } else {
    snprintf(buf, sizeof(buf), "%d dBm", rssi_dbm);
  }
  u8g2.drawStr(0, 55, buf);

  u8g2.sendBuffer();
}

void display_wifi_list(const char* const* ssids, const int* rssi, int count) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "WiFi Scan");

  if (count > 5) count = 5;
  for (int i = 0; i < count; i++) {
    int y = 20 + i * 10;
    char ssid_buf[21] = {0};
    if (ssids && ssids[i]) {
      strncpy(ssid_buf, ssids[i], sizeof(ssid_buf) - 1);
    }
    if (ssid_buf[0] == '\0') {
      strncpy(ssid_buf, "(hidden)", sizeof(ssid_buf) - 1);
    }
    u8g2.drawStr(0, y, ssid_buf);

    if (rssi) {
      int v = rssi[i];
      int pct = (v >= -30) ? 100
              : (v <= -80) ? 0
              : ((v + 80) * 100) / 50;
      int bar = (pct * 6) / 100;
      u8g2.drawFrame(120, y - 8, 8, 8);
      if (bar > 0) u8g2.drawBox(122, y - 8, bar, 8);
    }
  }
  u8g2.sendBuffer();
}
