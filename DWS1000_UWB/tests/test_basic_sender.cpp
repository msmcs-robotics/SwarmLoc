/**
 * BasicSender - Direct from DW1000 library examples
 * Only change: baud rate 9600 -> 115200
 * Tests if the library's own example works with our SPI_EDGE fix.
 */
#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

boolean sent = false;
volatile boolean sentAck = false;
volatile unsigned long delaySent = 0;
int16_t sentNum = 0;
DW1000Time sentTime;

void handleSent() {
    sentAck = true;
}

void transmitter() {
    Serial.print("Transmitting packet ... #"); Serial.println(sentNum);
    DW1000.newTransmit();
    DW1000.setDefaults();
    String msg = "Hello DW1000, it's #"; msg += sentNum;
    DW1000.setData(msg);
    DW1000Time deltaTime = DW1000Time(10, DW1000Time::MILLISECONDS);
    DW1000.setDelay(deltaTime);
    DW1000.startTransmit();
    delaySent = millis();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("### BasicSender Example ###"));
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);
    Serial.println(F("DW1000 initialized"));
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(5);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();
    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print("Device ID: "); Serial.println(msg);
    DW1000.getPrintableDeviceMode(msg);
    Serial.print("Mode: "); Serial.println(msg);
    DW1000.attachSentHandler(handleSent);
    transmitter();
}

void loop() {
    if (!sentAck) return;
    sentAck = false;
    Serial.print("Sent #"); Serial.print(sentNum);
    Serial.print(" delay [ms]: "); Serial.println(millis() - delaySent);
    sentNum++;
    transmitter();
}
