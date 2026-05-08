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

void display_connected(const char* ssid,
                       const char* mac_str,
                       const char* ip_str) {
  u8g2.clearBuffer();

  // 3-row layout on 128x64. helvB12 = Helvetica Bold ~12pt, proportional.
  // Bold strokes survive 1-bit OLED rendering cleanly (the previous 7x14_tf
  // had too-thin strokes that looked broken). Proportional spacing means
  // a 17-char MAC like "0C:B8:15:C1:39:B8" fits horizontally because most
  // glyphs are narrower than 8 px.
  u8g2.setFont(u8g2_font_helvB12_tf);

  char buf[24];

  // Row 1 — SSID (baseline y=16, char body y≈4..16)
  snprintf(buf, sizeof(buf), "%.20s", (ssid && ssid[0]) ? ssid : "(no ssid)");
  u8g2.drawStr(0, 16, buf);

  // Row 2 — MAC (baseline y=38)
  snprintf(buf, sizeof(buf), "%.20s", (mac_str && mac_str[0]) ? mac_str : "(no mac)");
  u8g2.drawStr(0, 38, buf);

  // Row 3 — IP, only when actually connected (baseline y=60)
  if (ip_str && ip_str[0] && strcmp(ip_str, "0.0.0.0") != 0) {
    snprintf(buf, sizeof(buf), "%.20s", ip_str);
    u8g2.drawStr(0, 60, buf);
  }

  u8g2.sendBuffer();

  // Restore default font for other display calls.
  u8g2.setFont(u8g2_font_6x10_tf);
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
