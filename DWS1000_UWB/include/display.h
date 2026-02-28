#ifndef DISPLAY_H
#define DISPLAY_H

#ifdef USE_OLED_DISPLAY

#include <U8x8lib.h>
#include <Wire.h>
#include "config.h"

// SSD1306 128x32 (DSDTECH 0.91") — text mode (no frame buffer, saves RAM)
// 16 columns x 4 rows of 8x8 characters
static U8X8_SSD1306_128X32_UNIVISION_SW_I2C u8x8(OLED_SCL_PIN, OLED_SDA_PIN, U8X8_PIN_NONE);

inline void displayInit() {
    u8x8.begin();
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.clear();
    u8x8.drawString(0, 0, "DWS1000 UWB");
    u8x8.drawString(0, 1, "Initializing...");
}

inline void displayStatus(const char* line0, const char* line1 = nullptr,
                           const char* line2 = nullptr, const char* line3 = nullptr) {
    u8x8.clear();
    if (line0) u8x8.drawString(0, 0, line0);
    if (line1) u8x8.drawString(0, 1, line1);
    if (line2) u8x8.drawString(0, 2, line2);
    if (line3) u8x8.drawString(0, 3, line3);
}

// Display calibration progress: count, mean distance, error
inline void displayCalibration(int count, int total, float mean, float error) {
    char buf[17]; // 16 chars + null

    u8x8.clearLine(0);
    snprintf(buf, sizeof(buf), "CAL %d/%d", count, total);
    u8x8.drawString(0, 0, buf);

    u8x8.clearLine(1);
    dtostrf(mean, 5, 2, buf);
    strcat(buf, " m");
    u8x8.drawString(0, 1, buf);

    u8x8.clearLine(2);
    dtostrf(error * 100.0f, 5, 1, buf);
    strcat(buf, " cm err");
    u8x8.drawString(0, 2, buf);
}

// Display final calibration result
inline void displayCalResult(uint16_t antennaDelay, float error) {
    char buf[17];

    u8x8.clear();
    u8x8.drawString(0, 0, "CALIBRATED!");

    u8x8.clearLine(1);
    snprintf(buf, sizeof(buf), "Delay: %u", antennaDelay);
    u8x8.drawString(0, 1, buf);

    u8x8.clearLine(2);
    dtostrf(error * 100.0f, 5, 1, buf);
    strcat(buf, " cm err");
    u8x8.drawString(0, 2, buf);

    u8x8.drawString(0, 3, "Copy to config.h");
}

// Display live ranging distance
inline void displayDistance(float distance, int rangeCount) {
    char buf[17];

    u8x8.clearLine(0);
    snprintf(buf, sizeof(buf), "LIVE  R#%d", rangeCount);
    u8x8.drawString(0, 0, buf);

    u8x8.clearLine(1);
    dtostrf(distance, 6, 2, buf);
    strcat(buf, " m");
    u8x8.drawString(0, 1, buf);

    u8x8.clearLine(2);
    dtostrf(distance * 100.0f, 6, 1, buf);
    strcat(buf, " cm");
    u8x8.drawString(0, 2, buf);
}

#else // !USE_OLED_DISPLAY

// No-op stubs when OLED is disabled
inline void displayInit() {}
inline void displayStatus(const char*, const char* = nullptr,
                           const char* = nullptr, const char* = nullptr) {}
inline void displayCalibration(int, int, float, float) {}
inline void displayCalResult(uint16_t, float) {}
inline void displayDistance(float, int) {}

#endif // USE_OLED_DISPLAY

#endif // DISPLAY_H
