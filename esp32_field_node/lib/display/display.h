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

// Render the 3-row connection-info screen: SSID, MAC, IP.
// Larger font; intentionally minimal.
//   ssid     — top row; "(no ssid)" if null/empty
//   mac_str  — middle row; usually WiFi.macAddress()
//   ip_str   — bottom row; pass nullptr / "" / "0.0.0.0" when not connected
//              and the row is left blank (IP earned only when connected)
//
// RSSI and captive-portal status are intentionally NOT rendered — they're
// printed to Serial only. The display stays clean: 3 facts, big font.
void display_connected(const char* ssid,
                       const char* mac_str,
                       const char* ip_str);

#endif
