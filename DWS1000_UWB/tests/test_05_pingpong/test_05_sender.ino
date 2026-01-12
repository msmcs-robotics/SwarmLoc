/*
 * Test 05: MessagePingPong - SENDER/INITIATOR
 *
 * Based on: lib/DW1000/examples/MessagePingPong/MessagePingPong.ino
 *
 * Purpose: Test basic DW1000 message communication (no ranging)
 *
 * Hardware: Arduino Uno + PCL298336 v1.3 (DW1000)
 * Upload to: /dev/ttyACM0 (or first Arduino)
 *
 * This device INITIATES the ping-pong by sending first message.
 *
 * Expected Behavior:
 * 1. Sends "Ping ..." message
 * 2. Waits for "... and Pong" response
 * 3. Receives response and sends another ping
 * 4. Continues ping-pong exchange
 *
 * Test Date: 2026-01-11
 */

#include "require_cpp11.h"
#include <SPI.h>
#include <DW1000.h>

// Forward declarations
void handleSent();
void handleReceived();
void handleReceiveFailed();
void transmit();
void receiver();

// connection pins
constexpr uint8_t PIN_RST = 9; // reset pin
constexpr uint8_t PIN_IRQ = 2; // irq pin
constexpr uint8_t PIN_SS = SS; // spi select pin

// toggle state
enum class TransmissionState : uint8_t {
  SENDER,
  RECEIVER
};

// toggle and message RX/TX
// NOTE: Configured as SENDER to initiate ping-pong
TransmissionState trxToggle = TransmissionState::SENDER;
volatile boolean trxAck = false;
volatile boolean rxError = false;
String msg;

// Counter for debugging
unsigned long messageCount = 0;

void setup() {
  // DEBUG monitoring
  Serial.begin(9600);
  Serial.println(F("### DW1000 MessagePingPong Test - SENDER/INITIATOR ###"));
  Serial.println(F("Test 05: Basic Message Communication"));
  Serial.println();

  // initialize the driver
  Serial.print(F("Initializing DW1000... "));
  DW1000.begin(PIN_IRQ, PIN_RST);
  DW1000.select(PIN_SS);
  Serial.println(F("OK"));

  // general configuration
  Serial.print(F("Configuring DW1000... "));
  DW1000.newConfiguration();
  DW1000.setDefaults();
  DW1000.setDeviceAddress(1);
  DW1000.setNetworkId(10);
  DW1000.commitConfiguration();
  Serial.println(F("OK"));

  // DEBUG chip info and registers pretty printed
  char msgInfo[128];
  DW1000.getPrintableDeviceIdentifier(msgInfo);
  Serial.print(F("Device ID: ")); Serial.println(msgInfo);
  DW1000.getPrintableExtendedUniqueIdentifier(msgInfo);
  Serial.print(F("Unique ID: ")); Serial.println(msgInfo);
  DW1000.getPrintableNetworkIdAndShortAddress(msgInfo);
  Serial.print(F("Network ID & Device Address: ")); Serial.println(msgInfo);
  DW1000.getPrintableDeviceMode(msgInfo);
  Serial.print(F("Device mode: ")); Serial.println(msgInfo);
  Serial.println();

  // attach callback for (successfully) sent and received messages
  DW1000.attachSentHandler(handleSent);
  DW1000.attachReceivedHandler(handleReceived);
  DW1000.attachReceiveFailedHandler(handleReceiveFailed);

  // sender starts by sending a PING message
  Serial.println(F("*** SENDER MODE: Initiating ping-pong ***"));
  Serial.println(F("Waiting for receiver to be ready..."));
  delay(2000); // Give receiver time to start

  msg = "Ping ...";
  receiver();
  Serial.println(F("Sending initial PING..."));
  transmit();
}

void handleSent() {
  // status change on sent success
  trxAck = true;
}

void handleReceived() {
  // status change on received success
  trxAck = true;
}

void handleReceiveFailed() {
  // error flag
  rxError = true;
}

void transmit() {
  DW1000.newTransmit();
  DW1000.setDefaults();
  DW1000.setData(msg);
  DW1000.startTransmit();
}

void receiver() {
  DW1000.newReceive();
  DW1000.setDefaults();
  // so we don't need to restart the receiver manually
  DW1000.receivePermanently(true);
  DW1000.startReceive();
}

void loop() {
  if (rxError) {
    Serial.println(F("ERROR: Failed to properly receive message."));
    rxError = false;
    return;
  }

  if (!trxAck) {
    return;
  }

  // continue on any success confirmation
  trxAck = false;
  messageCount++;

  // a sender will be a receiver and vice versa
  trxToggle = (trxToggle == TransmissionState::SENDER) ? TransmissionState::RECEIVER : TransmissionState::SENDER;

  if (trxToggle == TransmissionState::SENDER) {
    // formerly a receiver - we just received a pong
    String rxMsg;
    DW1000.getData(rxMsg);
    Serial.print(F("[RX #"));
    Serial.print(messageCount);
    Serial.print(F("] Received: \""));
    Serial.print(rxMsg);
    Serial.println(F("\""));

    // Now send another ping
    transmit();
  } else {
    // formerly a sender - we just sent a ping
    Serial.print(F("[TX #"));
    Serial.print(messageCount);
    Serial.print(F("] Transmitted: \""));
    Serial.print(msg);
    Serial.println(F("\""));
  }
}
