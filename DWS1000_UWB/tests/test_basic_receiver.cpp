/**
 * BasicReceiver - Direct from DW1000 library examples
 * Only change: baud rate 9600 -> 115200
 * Tests if the library's own example works with our SPI_EDGE fix.
 */
#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

volatile boolean received = false;
volatile boolean error = false;
volatile int16_t numReceived = 0;
String message;

void handleReceived() {
    received = true;
}

void handleError() {
    error = true;
}

void receiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("### BasicReceiver Example ###"));
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);
    Serial.println(F("DW1000 initialized"));
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(6);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();
    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print("Device ID: "); Serial.println(msg);
    DW1000.getPrintableDeviceMode(msg);
    Serial.print("Mode: "); Serial.println(msg);
    DW1000.attachReceivedHandler(handleReceived);
    DW1000.attachReceiveFailedHandler(handleError);
    DW1000.attachErrorHandler(handleError);
    receiver();
    Serial.println(F("Waiting for messages..."));
}

void loop() {
    if (received) {
        numReceived++;
        DW1000.getData(message);
        Serial.print("RX #"); Serial.print(numReceived);
        Serial.print(": "); Serial.println(message);
        Serial.print("  Power [dBm]: "); Serial.println(DW1000.getReceivePower());
        received = false;
    }
    if (error) {
        Serial.println("RX Error");
        error = false;
    }
}
