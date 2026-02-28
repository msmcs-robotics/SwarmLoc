#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// DWS1000_UWB Configuration
// =============================================================================
//
// This file contains calibration values and feature flags for the UWB ranging
// system. Values are set during calibration and should not be changed manually
// unless you know what you're doing.
//
// Workflow:
//   1. Flash calibration firmware (pio run -e uno_calibration -t upload)
//   2. Place radios at known distance, run calibration
//   3. Copy calibrated values here
//   4. Uncomment the CALIBRATED_* marker
//   5. Reflash live firmware (pio run -e uno_live -t upload)
//

// =============================================================================
// Calibration Stage Markers
// =============================================================================
// Uncomment each marker after completing that calibration stage.
// The firmware checks these to know what's been calibrated.

//#define CALIBRATED_ANTENNA_DELAY    // Stage 1: Antenna delay at known distance
//#define CALIBRATED_MULTI_DISTANCE   // Stage 2: Verified at multiple distances

// =============================================================================
// Hardware Configuration
// =============================================================================

// Pin assignments (DWS1000 shield on Arduino Uno)
// These use #ifndef guards so firmware files can override if needed.
#ifndef PIN_RST
#define PIN_RST         7       // DW1000 reset
#endif
#ifndef PIN_IRQ
#define PIN_IRQ         2       // DW1000 interrupt (wired from D8)
#endif
// Note: PIN_SS not defined here — Arduino core defines SS already

// I2C OLED display (optional, DSDTECH 0.91" SSD1306 128x32)
#define OLED_SDA_PIN    A4      // I2C data
#define OLED_SCL_PIN    A5      // I2C clock
#define OLED_ADDRESS    0x3C    // I2C address (try 0x3D if not found)

// Device role (set per device, or auto-detect)
// #define DEVICE_ROLE_ANCHOR
// #define DEVICE_ROLE_TAG

// =============================================================================
// Radio Configuration
// =============================================================================

#define RADIO_CHANNEL           5       // UWB channel (5 = 6.5 GHz)
#define RADIO_DATA_RATE         0       // 0=110kbps, 1=850kbps, 2=6.8Mbps
#define RADIO_PRF               0       // 0=16MHz, 1=64MHz
#define RADIO_PREAMBLE_LEN      256     // Preamble symbols
#define RADIO_PREAMBLE_CODE     3       // Preamble code

// =============================================================================
// Calibration Values — Antenna Delay
// =============================================================================
// The antenna delay compensates for RF propagation time inside the DW1000 chip.
// Default (uncalibrated): 16436
// Calibrated value is determined by measuring at a known distance.
//
// Formula: new_delay = old_delay + (measured_error / 2) / DISTANCE_OF_RADIO
//   where DISTANCE_OF_RADIO = 0.004692 m/tick
//
// Sign: If measuring TOO LONG → decrease delay. TOO SHORT → increase delay.

#define ANTENNA_DELAY_DEFAULT   16436   // Factory default (uncalibrated)
#define ANTENNA_DELAY           16405   // Calibrated value (-31 ticks from default)

// Per-device LDO tuning (from OTP or manual calibration)
#define LDO_TUNE_DEV0           0x88    // Anchor (ACM0) — better RX
#define LDO_TUNE_DEV1           0x28    // Tag (ACM1)

// =============================================================================
// Calibration Values — Distance Offsets (future)
// =============================================================================
// Per-distance linear correction: corrected = raw * SCALE + OFFSET
// Set after multi-distance validation.

#define DISTANCE_SCALE          1.0f    // Multiplicative correction
#define DISTANCE_OFFSET         0.0f   // Additive correction (meters)

// =============================================================================
// Feature Flags
// =============================================================================

// #define USE_OLED_DISPLAY         // Enable OLED output (SSD1306 128x32)
// #define USE_OUTLIER_FILTER       // Enable statistical outlier rejection
// #define USE_MOVING_AVERAGE       // Enable moving average smoothing

// Outlier filter settings (if USE_OUTLIER_FILTER defined)
#define OUTLIER_THRESHOLD_M     2.0f    // Reject readings > this far from mean
#define MOVING_AVG_WINDOW       5       // Samples for moving average

// =============================================================================
// Debug
// =============================================================================

// #define DEBUG_TIMESTAMPS         // Print raw DW1000 timestamps
// #define DEBUG_SPI                // Print SPI register reads

#endif // CONFIG_H
