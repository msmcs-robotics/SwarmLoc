/*
 * ESP32 DWM3000 Test Code Template
 *
 * Hardware: ESP32 DevKit + Qorvo PCL298336 DWM3000EVB Shield
 *
 * Wiring (8 connections required):
 *   Shield D11 (MOSI) → ESP32 GPIO 23
 *   Shield D12 (MISO) → ESP32 GPIO 19
 *   Shield D13 (SCK)  → ESP32 GPIO 18
 *   Shield D10 (CS)   → ESP32 GPIO 5
 *   Shield D2  (IRQ)  → ESP32 GPIO 4
 *   Shield D9  (RST)  → ESP32 GPIO 16
 *   Shield 3.3V       → ESP32 3.3V
 *   Shield GND        → ESP32 GND
 *
 * Library: https://github.com/Fhilb/DW3000_Arduino
 *
 * PlatformIO Setup:
 *   platformio.ini should include:
 *   lib_deps = https://github.com/Fhilb/DW3000_Arduino.git
 *
 * Upload and open Serial Monitor at 115200 baud
 */

#include <SPI.h>
// #include <DW3000.h>  // Include your DW3000 library here

// ============================================================================
// PIN DEFINITIONS FOR ESP32 + PCL298336 SHIELD
// ============================================================================

// SPI pins (VSPI on ESP32)
#define PIN_SCK     18    // VSPI SCK  (Shield D13)
#define PIN_MISO    19    // VSPI MISO (Shield D12)
#define PIN_MOSI    23    // VSPI MOSI (Shield D11)

// Control pins (configurable)
#define PIN_CS      5     // Chip Select (Shield D10)
#define PIN_IRQ     4     // Interrupt Request (Shield D2)
#define PIN_RST     16    // Reset (Shield D9)

// SPI settings
#define SPI_SPEED   4000000  // 4 MHz (safe starting point, can increase to 8MHz)

// ============================================================================
// DEVICE CONFIGURATION
// ============================================================================

// Device role (set based on which device this is)
#define DEVICE_ROLE_INITIATOR  1
#define DEVICE_ROLE_RESPONDER  2

// Set this to match the device:
#define DEVICE_ROLE  DEVICE_ROLE_INITIATOR  // Change to RESPONDER for second device

// Device addressing
#define DEVICE_ADDRESS_INIT  0x01
#define DEVICE_ADDRESS_RESP  0x02

#if DEVICE_ROLE == DEVICE_ROLE_INITIATOR
  #define MY_ADDRESS     DEVICE_ADDRESS_INIT
  #define TARGET_ADDRESS DEVICE_ADDRESS_RESP
#else
  #define MY_ADDRESS     DEVICE_ADDRESS_RESP
  #define TARGET_ADDRESS DEVICE_ADDRESS_INIT
#endif

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

volatile bool interruptReceived = false;

// ============================================================================
// INTERRUPT HANDLER
// ============================================================================

void IRAM_ATTR handleDW3000Interrupt() {
    interruptReceived = true;
}

// ============================================================================
// HARDWARE RESET SEQUENCE
// ============================================================================

void resetDW3000() {
    Serial.println("Performing hardware reset...");

    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, LOW);
    delay(10);
    digitalWrite(PIN_RST, HIGH);
    delay(10);

    Serial.println("Reset complete");
}

// ============================================================================
// SPI INITIALIZATION
// ============================================================================

void initializeSPI() {
    Serial.println("Initializing SPI...");

    // Configure CS pin
    pinMode(PIN_CS, OUTPUT);
    digitalWrite(PIN_CS, HIGH);  // CS idle high

    // Initialize SPI bus
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    Serial.println("SPI initialized");
}

// ============================================================================
// DW3000 VERIFICATION (READ DEVICE ID)
// ============================================================================

// Basic SPI transaction to read device ID register
// You'll need to replace this with actual library calls
uint32_t readDeviceID() {
    // This is a placeholder - use your DW3000 library's method
    // Expected value: 0xDECA0302 for DW3000

    // Example structure (adapt to your library):
    // digitalWrite(PIN_CS, LOW);
    // SPI.transfer(0x00);  // Read command + register address
    // uint32_t id = SPI.transfer32(0x00);
    // digitalWrite(PIN_CS, HIGH);
    // return id;

    Serial.println("WARNING: readDeviceID() is placeholder - implement with library");
    return 0x00000000;  // Placeholder
}

// ============================================================================
// DW3000 INITIALIZATION
// ============================================================================

bool initializeDW3000() {
    Serial.println("========================================");
    Serial.println("Initializing DW3000 Module...");
    Serial.println("========================================");

    // Step 1: Hardware reset
    resetDW3000();

    // Step 2: Initialize SPI
    initializeSPI();

    // Step 3: Configure interrupt pin
    pinMode(PIN_IRQ, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_IRQ), handleDW3000Interrupt, RISING);
    Serial.println("Interrupt attached to GPIO 4");

    // Step 4: Initialize DW3000 library
    // Replace with your library's initialization:
    // DW3000.begin(PIN_IRQ, PIN_RST);
    // DW3000.select(PIN_CS);

    delay(100);

    // Step 5: Verify communication by reading device ID
    Serial.print("Reading device ID... ");
    uint32_t deviceID = readDeviceID();
    Serial.print("0x");
    Serial.println(deviceID, HEX);

    if (deviceID == 0xDECA0302) {
        Serial.println("✓ DWM3000 detected!");
        Serial.println("SPI communication: OK");
        return true;
    } else if (deviceID == 0x00000000 || deviceID == 0xFFFFFFFF) {
        Serial.println("✗ Communication FAILED");
        Serial.println("Check wiring:");
        Serial.println("  - MOSI (Shield D11) → GPIO 23");
        Serial.println("  - MISO (Shield D12) → GPIO 19");
        Serial.println("  - SCK  (Shield D13) → GPIO 18");
        Serial.println("  - CS   (Shield D10) → GPIO 5");
        Serial.println("  - 3.3V → 3.3V");
        Serial.println("  - GND  → GND");
        return false;
    } else {
        Serial.print("✗ Unexpected device ID: 0x");
        Serial.println(deviceID, HEX);
        Serial.println("Expected: 0xDECA0302 (DW3000)");
        return false;
    }
}

// ============================================================================
// DW3000 CONFIGURATION
// ============================================================================

void configureDW3000() {
    Serial.println("Configuring DW3000...");

    // Replace with your library's configuration methods:

    // Example configuration structure (adapt to your library):
    /*
    DW3000.newConfiguration();
    DW3000.setDefaults();
    DW3000.setDeviceAddress(MY_ADDRESS);
    DW3000.setNetworkId(0xDECA);
    DW3000.enableMode(DW3000.MODE_LONGDATA_RANGE_ACCURACY);

    // Optional but recommended:
    DW3000.setChannel(5);              // Channel 5 (6.5 GHz)
    DW3000.setPreambleLength(128);     // Balance of range and speed
    DW3000.setAntennaDelay(16450);     // Default, calibrate later

    DW3000.commitConfiguration();
    */

    Serial.println("Configuration complete");
    Serial.print("Device Address: 0x");
    Serial.println(MY_ADDRESS, HEX);
    Serial.print("Target Address: 0x");
    Serial.println(TARGET_ADDRESS, HEX);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    delay(1000);  // Wait for serial to stabilize

    Serial.println();
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║  ESP32 DWM3000 UWB Ranging System     ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println();

    // Print device role
    Serial.println("Device Configuration:");
    Serial.println("────────────────────────────────────────");
    #if DEVICE_ROLE == DEVICE_ROLE_INITIATOR
        Serial.println("Role: INITIATOR (Master)");
        Serial.println("Function: Starts ranging requests");
    #else
        Serial.println("Role: RESPONDER (Slave)");
        Serial.println("Function: Responds to ranging requests");
    #endif
    Serial.println();

    // Print pin configuration
    Serial.println("Pin Configuration:");
    Serial.println("────────────────────────────────────────");
    Serial.print("MOSI (D11) → GPIO ");
    Serial.println(PIN_MOSI);
    Serial.print("MISO (D12) → GPIO ");
    Serial.println(PIN_MISO);
    Serial.print("SCK  (D13) → GPIO ");
    Serial.println(PIN_SCK);
    Serial.print("CS   (D10) → GPIO ");
    Serial.println(PIN_CS);
    Serial.print("IRQ  (D2)  → GPIO ");
    Serial.println(PIN_IRQ);
    Serial.print("RST  (D9)  → GPIO ");
    Serial.println(PIN_RST);
    Serial.println();

    // Initialize DW3000
    if (!initializeDW3000()) {
        Serial.println("════════════════════════════════════════");
        Serial.println("INITIALIZATION FAILED!");
        Serial.println("System halted. Check connections.");
        Serial.println("════════════════════════════════════════");
        while (1) {
            delay(1000);  // Halt
        }
    }

    // Configure DW3000 for ranging
    configureDW3000();

    Serial.println();
    Serial.println("════════════════════════════════════════");
    Serial.println("Initialization Complete!");
    Serial.println("System Ready");
    Serial.println("════════════════════════════════════════");
    Serial.println();

    #if DEVICE_ROLE == DEVICE_ROLE_INITIATOR
        Serial.println("Waiting 3 seconds before starting ranging...");
        delay(3000);
        Serial.println("Starting ranging sequence...");
    #else
        Serial.println("Listening for ranging requests...");
    #endif
    Serial.println();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Check for interrupts
    if (interruptReceived) {
        interruptReceived = false;

        Serial.println("Interrupt received!");

        // Process interrupt based on device role
        // Replace with your library's interrupt handling

        #if DEVICE_ROLE == DEVICE_ROLE_INITIATOR
            // Handle received response
            // Calculate distance
            // Print result
            Serial.println("Initiator: Process response");
        #else
            // Handle received request
            // Send response
            Serial.println("Responder: Send response");
        #endif
    }

    #if DEVICE_ROLE == DEVICE_ROLE_INITIATOR
        // Initiator: Send ranging request periodically
        static unsigned long lastRangingTime = 0;
        unsigned long currentTime = millis();

        if (currentTime - lastRangingTime >= 1000) {  // Every 1 second
            lastRangingTime = currentTime;

            Serial.println("Initiator: Sending ranging request...");

            // Replace with your library's ranging request function
            // sendRangingRequest(TARGET_ADDRESS);
        }
    #endif

    // Small delay to prevent CPU hogging
    delay(10);
}

// ============================================================================
// HELPER FUNCTIONS (TO BE IMPLEMENTED)
// ============================================================================

/*
 * These are placeholder functions that you'll implement using your DW3000 library
 */

void sendRangingRequest(uint16_t targetAddress) {
    // Implement using your library
    // Example structure:
    // 1. Prepare poll message
    // 2. Capture TX timestamp (T1)
    // 3. Transmit poll
    // 4. Wait for response
    Serial.println("TODO: Implement sendRangingRequest()");
}

void handleRangingRequest() {
    // Implement using your library
    // Example structure:
    // 1. Receive poll message
    // 2. Capture RX timestamp (T2)
    // 3. Prepare response with T2, T3
    // 4. Transmit response
    Serial.println("TODO: Implement handleRangingRequest()");
}

float calculateDistance(uint64_t T1, uint64_t T2, uint64_t T3, uint64_t T4) {
    // Implement distance calculation
    // Using Double-Sided Two-Way Ranging formula:
    //
    // Round1 = T4 - T1  (time on initiator)
    // Reply1 = T3 - T2  (time on responder)
    //
    // For full DS-TWR with T5-T8:
    // Round2 = T8 - T5
    // Reply2 = T7 - T6
    // ToF = (Round1 * Round2 - Reply1 * Reply2) / (Round1 + Round2 + Reply1 + Reply2)
    //
    // Distance = ToF * SPEED_OF_LIGHT * DW3000_TIME_UNIT

    Serial.println("TODO: Implement calculateDistance()");
    return 0.0;
}

// ============================================================================
// DEBUGGING FUNCTIONS
// ============================================================================

void printTimestamp(const char* label, uint64_t timestamp) {
    Serial.print(label);
    Serial.print(": 0x");
    Serial.println((unsigned long)timestamp, HEX);
}

void printDistance(float distance) {
    Serial.print("Distance: ");
    Serial.print(distance, 2);
    Serial.println(" meters");
}

void testInterruptPin() {
    // Test if interrupt pin is functioning
    Serial.println("Testing interrupt pin...");
    Serial.print("IRQ pin (GPIO ");
    Serial.print(PIN_IRQ);
    Serial.print(") state: ");
    Serial.println(digitalRead(PIN_IRQ) ? "HIGH" : "LOW");
}

void testSPICommunication() {
    // Perform basic SPI test
    Serial.println("Testing SPI communication...");

    digitalWrite(PIN_CS, LOW);
    delay(1);
    SPI.transfer(0x00);  // Dummy byte
    digitalWrite(PIN_CS, HIGH);

    Serial.println("SPI test complete");
}

// ============================================================================
// VOLTAGE MONITORING (Optional - for debugging power issues)
// ============================================================================

void checkPowerSupply() {
    // Note: ESP32 doesn't have built-in voltage measurement of 3.3V rail
    // You'd need external circuitry or multimeter
    // This is a placeholder for documentation
    Serial.println("Power Check:");
    Serial.println("  Use multimeter to verify:");
    Serial.println("  - 3.3V at DWM3000 VCC pin");
    Serial.println("  - Should read 3.2-3.4V");
    Serial.println("  - Should not drop below 3.2V during TX");
}

// ============================================================================
// NOTES AND TROUBLESHOOTING
// ============================================================================

/*
 * TROUBLESHOOTING GUIDE:
 *
 * 1. Device ID reads 0x00000000 or 0xFFFFFFFF:
 *    - Check all SPI connections (MOSI, MISO, SCK, CS)
 *    - Verify MOSI and MISO are not swapped
 *    - Check 3.3V power at shield
 *    - Try lower SPI speed (1 MHz)
 *
 * 2. No interrupt firing:
 *    - Check IRQ wire connection
 *    - Verify interrupt attached in code
 *    - Test IRQ pin state with testInterruptPin()
 *
 * 3. Module resets or unstable:
 *    - Add decoupling capacitors (10µF + 100nF)
 *    - Use powered USB hub
 *    - Check voltage doesn't drop below 3.2V
 *
 * 4. Poor accuracy:
 *    - Calibrate antenna delay
 *    - Test in open area (reduce multipath)
 *    - Ensure line-of-sight between devices
 *
 * EXPECTED RESULTS:
 * - Device ID should read: 0xDECA0302
 * - Ranging accuracy: ±5-10 cm (after calibration)
 * - Update rate: 1-10 Hz typical
 *
 * WIRING VERIFICATION:
 * Use multimeter in continuity mode to verify each connection:
 *   Shield Pin    ESP32 Pin
 *   ──────────────────────
 *   D11 (MOSI) ─> GPIO 23
 *   D12 (MISO) ─> GPIO 19
 *   D13 (SCK)  ─> GPIO 18
 *   D10 (CS)   ─> GPIO 5
 *   D2  (IRQ)  ─> GPIO 4
 *   D9  (RST)  ─> GPIO 16
 *   3.3V       ─> 3.3V
 *   GND        ─> GND
 *
 * LIBRARY SETUP:
 * 1. Add to platformio.ini:
 *    lib_deps = https://github.com/Fhilb/DW3000_Arduino.git
 *
 * 2. Include in code:
 *    #include <DW3000.h>
 *
 * 3. Follow library documentation for:
 *    - Initialization sequence
 *    - Configuration options
 *    - Ranging protocol implementation
 *
 * NEXT STEPS:
 * 1. Verify SPI communication (device ID read)
 * 2. Test interrupt functionality
 * 3. Implement basic TX/RX
 * 4. Implement full TWR protocol
 * 5. Calibrate antenna delay
 * 6. Test at known distances
 * 7. Refine accuracy
 */

// ============================================================================
// END OF FILE
// ============================================================================
