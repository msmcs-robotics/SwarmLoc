#include "imu.h"

#include <Arduino.h>
#include <Wire.h>
#include "MPU6050.h"

// Default address (AD0 = LOW = 0x68). The MPU6050 constructor accepts an
// address arg; default is 0x68 so we don't need to pass anything.
static MPU6050 g_mpu;
static bool    g_init = false;

// Convert raw int16 ADC counts → physical units. Defaults assumed: ±2g and
// ±250 °/s range as set in imu_init() below.
static constexpr float ACCEL_LSB_PER_G   = 16384.0f;  // ±2 g full scale
static constexpr float GYRO_LSB_PER_DPS  = 131.0f;    // ±250 °/s full scale

bool imu_init() {
  // Wire.begin() must have been called by the caller (main.cpp does this
  // before scanning the I2C bus).
  g_mpu.initialize();
  g_init = g_mpu.testConnection();

  if (g_init) {
    g_mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);
    g_mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_250);
    Serial.println("[imu] MPU6050 detected and configured (±2 g, ±250 °/s)");
  } else {
    Serial.println("[imu] MPU6050 NOT detected at 0x68 — check wiring");
  }
  return g_init;
}

bool imu_ready() { return g_init; }

bool imu_read(ImuReading* out) {
  if (!g_init || !out) return false;

  int16_t ax, ay, az, gx, gy, gz;
  g_mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  int16_t t = g_mpu.getTemperature();

  out->accel_x = ax / ACCEL_LSB_PER_G;
  out->accel_y = ay / ACCEL_LSB_PER_G;
  out->accel_z = az / ACCEL_LSB_PER_G;
  out->gyro_x  = gx / GYRO_LSB_PER_DPS;
  out->gyro_y  = gy / GYRO_LSB_PER_DPS;
  out->gyro_z  = gz / GYRO_LSB_PER_DPS;
  // Datasheet formula: T(°C) = (TEMP_OUT / 340) + 36.53
  out->temp_c       = (t / 340.0f) + 36.53f;
  out->millis_when  = millis();
  return true;
}
