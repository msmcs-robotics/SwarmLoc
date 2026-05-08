# MPU6050 wiring for esp32_field_node (M4 prep)

> Created: 2026-05-08
> Status: hardware not yet wired; this is the reference for when it is

## Pinout — MPU6050 breakout to ESP32-WROOM-32D DevKit

The MPU6050 is an I2C device. It shares the **same I2C bus as the OLED**
(SDA=GPIO21, SCL=GPIO22). Different I2C addresses (0x3C OLED vs 0x68 IMU)
mean both coexist without conflict.

| MPU6050 pin | ESP32 GPIO | Required? | Notes |
|-------------|------------|-----------|-------|
| **VCC**  | 3V3        | required | Most breakouts have an onboard regulator that accepts 3.3–5 V. **3.3V is safest** with the ESP32. |
| **GND**  | GND        | required | — |
| **SCL**  | GPIO 22    | required | Same line as OLED SCL. Use a breadboard rail. |
| **SDA**  | GPIO 21    | required | Same line as OLED SDA. Use a breadboard rail. |
| **AD0**  | GND or NC  | optional | Selects I2C address: tied LOW (or open) → **0x68** (default). Tied HIGH → 0x69. Leave open for default. |
| **INT**  | GPIO 34    | optional | Data-ready interrupt. Not needed for polling-mode firmware. GPIO34 is input-only; safe choice. |
| **XDA**  | (open)     | unused | Auxiliary I2C master line for slave magnetometer; we don't have one. |
| **XCL**  | (open)     | unused | Auxiliary I2C clock; same. |
| **FSYNC**| (open)     | unused | Frame-sync input; unused. |

So the minimum wiring is just **4 wires**: VCC, GND, SDA, SCL. Both INT
and AD0 are optional.

## Verification after wiring

Once wired, the existing I2C scanner in `src/main.cpp` will detect the
chip on first boot:

```
[i2c]   device @ 0x3C  -- SSD1306 OLED
[i2c]   device @ 0x68  -- MPU6050 IMU
[i2c] scan done, 2 device(s) found
```

If you see `device @ 0x69` instead, the AD0 pad is bridged HIGH. Either
clear the bridge or set the IMU library's address to 0x69.

## Library options for M4

When we get to writing the IMU code, two reasonable choices:

1. **`adafruit/Adafruit MPU6050`** + **`adafruit/Adafruit Unified Sensor`**
   - Simple, well-documented API: `mpu.begin()`, `mpu.getEvent(&accel, &gyro, &temp)`
   - Returns SI units, easy to work with
   - Heavier (~10 KB flash for the library + Adafruit_BusIO)

2. **`electroniccats/MPU6050`** (a maintained fork of Jeff Rowberg's classic library)
   - Lower-level: gives raw int16 readings + DMP support
   - DMP (Digital Motion Processor) does sensor fusion on the chip → quaternions / Euler angles for free
   - More setup code; pays off if we want orientation rather than raw accel/gyro

Recommendation: start with **Adafruit_MPU6050** for simplicity. Switch to
electroniccats/MPU6050 only if we want DMP-based fusion later.

## Power-supply note

The MPU6050 draws < 5 mA. The ESP32 DevKit's onboard 3V3 regulator is
spec'd at 600+ mA. Plenty of headroom — no separate supply needed.

## What we DON'T need for M4

- A separate I2C bus (`Wire1`) — the OLED bus is fine
- Pull-up resistors — the OLED breakout already has 4.7 kΩ pull-ups on
  SDA / SCL
- Level shifter — the MPU6050 breakout is 3.3 V native (matches ESP32)

## Next step (when hardware is on the breadboard)

1. Wire VCC / GND / SDA / SCL per the table above
2. Power-cycle the ESP32 and watch for `device @ 0x68 -- MPU6050 IMU`
   in the boot serial output
3. Add `adafruit/Adafruit MPU6050` to `platformio.ini` `lib_deps`
4. Implement `lib/imu/imu.{h,cpp}` mirroring the `lib/display/` shape
5. Update main.cpp to call `imu_init()` after `display_init()` and a new
   `imu_print()` periodic call
6. Wire IMU data into `display_status()` for at-a-glance roll/pitch
