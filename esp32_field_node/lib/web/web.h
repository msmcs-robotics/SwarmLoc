// Minimal HTTP server on port 80. Serves an embedded HTML page that polls
// /imu (JSON) at ~10 Hz to display live MPU6050 data. Built on the
// Arduino-ESP32 built-in `WebServer` library (synchronous, no extra
// dependencies). Lives on Core 1 (the WiFi-friendly core); the IMU task
// publishes snapshots into a mutex-protected slot via web_set_imu().

#ifndef ESP32_FIELD_NODE_WEB_H
#define ESP32_FIELD_NODE_WEB_H

#include "imu.h"

// Register handlers and call WebServer::begin(). Returns true if WiFi is
// connected and the server started.
bool web_init();

// Pump pending HTTP requests. Call from the loop on the WiFi core.
void web_handle();

// Publish the latest IMU snapshot. Mutex-protected — safe to call from
// the IMU task on a different core.
void web_set_imu(const ImuReading& r);

#endif
