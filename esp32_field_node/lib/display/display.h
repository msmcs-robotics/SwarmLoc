// SSD1306 128x64 OLED driver wrapper for esp32_field_node.
// Uses U8g2 hardware-I2C with the ESP32 default Wire pins (SDA=21, SCL=22).
// Library: olikraus/U8g2 (declared in platformio.ini).

#ifndef ESP32_FIELD_NODE_DISPLAY_H
#define ESP32_FIELD_NODE_DISPLAY_H

#include <U8g2lib.h>
#include <Wire.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

bool display_init();
void display_clear();
void display_banner(const char* line1, const char* line2 = nullptr);
void display_status(const char* status_line);
void display_wifi_list(const char* const* ssids, const int* rssi, int count);

// Render the post-connection info screen the user asked for: SSID, IP,
// MAC, RSSI, and an optional portal-status string.
//   mode_label   short label for top-line, e.g. "OPEN" or "WPA2-Ent"
//   portal_status one of "online" / "captive" / "noprobe" / "error" / nullptr
void display_connected(const char* mode_label,
                       const char* ssid,
                       const char* ip_str,
                       const char* mac_str,
                       int         rssi_dbm,
                       const char* portal_status);

#endif
