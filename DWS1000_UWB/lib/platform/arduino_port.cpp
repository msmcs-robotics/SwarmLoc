/**
 * @file arduino_port.cpp
 * @brief Arduino Platform Implementation for DWM3000
 */

#include "arduino_port.h"

// SPI settings
static SPISettings spiSettingsSlow(DW_SPI_SLOW_RATE, MSBFIRST, SPI_MODE0);
static SPISettings spiSettingsFast(DW_SPI_FAST_RATE, MSBFIRST, SPI_MODE0);
static SPISettings* currentSettings = &spiSettingsSlow;

/**
 * @brief Initialize SPI and GPIO pins for DWM3000
 */
void arduino_spi_init(void) {
    // Initialize SPI
    SPI.begin();

    // Setup CS pin
    pinMode(DW_CS_PIN, OUTPUT);
    digitalWrite(DW_CS_PIN, HIGH);

    // Setup Reset pin
    pinMode(DW_RST_PIN, OUTPUT);
    digitalWrite(DW_RST_PIN, HIGH);

    // Setup IRQ pin (input with pullup)
    pinMode(DW_IRQ_PIN, INPUT_PULLUP);

    // Start with slow SPI rate
    arduino_set_spi_slow();
}

/**
 * @brief Hardware reset of DWM3000 module
 */
void arduino_dw_reset(void) {
    digitalWrite(DW_RST_PIN, LOW);
    delay(10);
    digitalWrite(DW_RST_PIN, HIGH);
    delay(10);
}

/**
 * @brief Set SPI to slow rate (for initialization)
 */
void arduino_set_spi_slow(void) {
    currentSettings = &spiSettingsSlow;
}

/**
 * @brief Set SPI to fast rate (for normal operation)
 */
void arduino_set_spi_fast(void) {
    currentSettings = &spiSettingsFast;
}

/**
 * @brief Read from DWM3000 via SPI
 *
 * @param headerLength Length of header to send
 * @param headerBuffer Header data buffer
 * @param readLength Length of data to read
 * @param readBuffer Buffer to store read data
 * @return 0 on success
 */
int arduino_spi_read(uint16_t headerLength, uint8_t *headerBuffer,
                     uint16_t readLength, uint8_t *readBuffer) {
    SPI.beginTransaction(*currentSettings);
    digitalWrite(DW_CS_PIN, LOW);

    // Send header
    for (uint16_t i = 0; i < headerLength; i++) {
        SPI.transfer(headerBuffer[i]);
    }

    // Read data
    for (uint16_t i = 0; i < readLength; i++) {
        readBuffer[i] = SPI.transfer(0);
    }

    digitalWrite(DW_CS_PIN, HIGH);
    SPI.endTransaction();

    return 0;
}

/**
 * @brief Write to DWM3000 via SPI
 *
 * @param headerLength Length of header to send
 * @param headerBuffer Header data buffer
 * @param bodyLength Length of body data to write
 * @param bodyBuffer Body data buffer
 * @return 0 on success
 */
int arduino_spi_write(uint16_t headerLength, uint8_t *headerBuffer,
                      uint16_t bodyLength, uint8_t *bodyBuffer) {
    SPI.beginTransaction(*currentSettings);
    digitalWrite(DW_CS_PIN, LOW);

    // Send header
    for (uint16_t i = 0; i < headerLength; i++) {
        SPI.transfer(headerBuffer[i]);
    }

    // Send body
    for (uint16_t i = 0; i < bodyLength; i++) {
        SPI.transfer(bodyBuffer[i]);
    }

    digitalWrite(DW_CS_PIN, HIGH);
    SPI.endTransaction();

    return 0;
}
