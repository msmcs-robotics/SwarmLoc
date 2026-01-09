/**
 * @file arduino_port.h
 * @brief Arduino/PlatformIO Platform Adaptation Layer for DWM3000
 *
 * This header bridges the gap between the bare-metal ATmega328p library
 * and the Arduino framework, providing Arduino-compatible implementations
 * of the platform-specific functions.
 */

#ifndef ARDUINO_PORT_H_
#define ARDUINO_PORT_H_

#include <Arduino.h>
#include <SPI.h>

#ifdef __cplusplus
extern "C" {
#endif

// Pin definitions for Arduino Uno with DWM3000 shield
#define DW_CS_PIN    10
#define DW_IRQ_PIN   2
#define DW_RST_PIN   9

// SPI Settings for DWM3000
#define DW_SPI_SLOW_RATE  2000000   // 2 MHz for initialization
#define DW_SPI_FAST_RATE  8000000   // 8 MHz for normal operation

// Boolean definitions
#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

// Arduino-compatible platform functions
void arduino_spi_init(void);
void arduino_dw_reset(void);
void arduino_set_spi_slow(void);
void arduino_set_spi_fast(void);
int arduino_spi_read(uint16_t headerLength, uint8_t *headerBuffer,
                     uint16_t readLength, uint8_t *readBuffer);
int arduino_spi_write(uint16_t headerLength, uint8_t *headerBuffer,
                      uint16_t bodyLength, uint8_t *bodyBuffer);

// Sleep functions (Arduino-compatible)
static inline void sleepms(uint32_t ms) {
    delay(ms);
}

static inline void deca_sleep(uint32_t ms) {
    delay(ms);
}

static inline void deca_usleep(uint32_t us) {
    delayMicroseconds(us);
}

#ifdef __cplusplus
}
#endif

#endif /* ARDUINO_PORT_H_ */
