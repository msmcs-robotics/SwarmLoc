// Thin wrapper around the bundled i2cdevlib MPU6050 (lib/MPU6050/).
// Initializes the chip at I2C 0x68 with default ranges (±2 g, ±250 °/s)
// and exposes a clean struct for downstream consumers (web, serial CLI).

#ifndef ESP32_FIELD_NODE_IMU_H
#define ESP32_FIELD_NODE_IMU_H

#include <stdint.h>

struct ImuReading {
  float    accel_x, accel_y, accel_z;  // g (1 g = 9.81 m/s²)
  float    gyro_x,  gyro_y,  gyro_z;   // °/s
  float    temp_c;                     // °C
  uint32_t millis_when;                // millis() at sample time
};

// Initialize MPU6050. Wire.begin() must already have been called.
// Returns true if the chip responds to testConnection().
bool imu_init();

// Has imu_init() succeeded?
bool imu_ready();

// Read raw motion + temp into `out`. Returns false if not initialized
// or if the read fails. Does NOT take any I2C mutex — callers on a
// multi-core system must serialize Wire access externally.
bool imu_read(ImuReading* out);

#endif
