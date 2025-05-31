/*
 * LoRa Node 2 - Adafruit Huzzah32 + RFM95W FeatherWing
 * Waits for messages, then responds when receiving them
 * 
 * Required Libraries:
 * - LoRa library by Sandeep Mistry (install via Library Manager)
 * 
 * Wiring (FeatherWing should auto-connect when stacked):
 * RFM95W FeatherWing -> Huzzah32
 * CS   -> Pin 33 (default for FeatherWing)
 * RST  -> Pin 27 (default for FeatherWing) 
 * INT  -> Pin 12 (default for FeatherWing)
 * SCK  -> Pin 5  (SPI)
 * MISO -> Pin 19 (SPI)
 * MOSI -> Pin 18 (SPI)
 */

#include <SPI.h>
#include <LoRa.h>

// FeatherWing pin definitions for Huzzah32
#define LORA_CS     33
#define LORA_RST    27
#define LORA_IRQ    12

// LoRa frequency
#define LORA_FREQ   915E6

// Node identification and timing
const char* nodeID = "LoRa2";
unsigned long responseDelay = 2500;      // Fixed delay before responding (longer for slave)
unsigned long radioSettleTime = 150;    // Time to let radio settle after TX
unsigned long preTransmitDelay = 50;    // Delay before transmission for clean state
int messageCounter = 0;
int expectedSequence = 1;               // Track expected sequence from Node 1
unsigned long lastReceiveTime = 0;
unsigned long processingStartTime = 0;   // Track processing time for ToF
bool radioInReceiveMode = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("LoRa Node 2 - Deterministic Ping-Pong Slave");
  Serial.println("============================================");

  // Setup LoRa transceiver module
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

  // Initialize LoRa with retries
  int retries = 0;
  while (!LoRa.begin(LORA_FREQ) && retries < 5) {
    Serial.println("LoRa init failed, retrying...");
    delay(1000);
    retries++;
  }
  
  if (retries >= 5) {
    Serial.println("LoRa initialization failed completely!");
    Serial.println("Check wiring and connections!");
    while (1);
  }
  
  Serial.println("LoRa init succeeded.");
  Serial.print("Frequency: ");
  Serial.print(LORA_FREQ / 1E6);
  Serial.println(" MHz");

  // Optimized LoRa parameters for reliability and ToF accuracy (must match Node 1)
  LoRa.setTxPower(17);              // Clean signal strength
  LoRa.setSpreadingFactor(8);       // Good sensitivity vs speed balance
  LoRa.setSignalBandwidth(125E3);   
  LoRa.setCodingRate4(8);           // Error correction for reliability
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  LoRa.enableInvertIQ(false);       // Consistent IQ setting

  Serial.println("LoRa parameters set:");
  Serial.println("- TX Power: 17 dBm");
  Serial.println("- Spreading Factor: 8");
  Serial.println("- Bandwidth: 125 kHz");
  Serial.println("- Coding Rate: 4/8");
  Serial.println("- CRC: Enabled");
  Serial.println();

  // Put radio in receive mode initially
  enterReceiveMode();
  
  Serial.println("LoRa2 (Slave) ready! Waiting for messages...");
  Serial.println();
}

void loop() {
  receiveMessage();
  delay(10); // Minimal delay for responsiveness
}

void enterReceiveMode() {
  if (!radioInReceiveMode) {
    LoRa.receive();
    radioInReceiveMode = true;
    delay(10); // Brief settle time for receive mode
  }
}

void sendMessage() {
  messageCounter++;
  String message = "PONG:" + String(nodeID) + ":" + String(messageCounter);
  
  // Exit receive mode before transmitting
  radioInReceiveMode = false;
  
  Serial.println("=== SENDING MESSAGE ===");
  Serial.print("Message: ");
  Serial.println(message);
  Serial.print("Sequence: ");
  Serial.println(messageCounter);
  
  // Record precise timing for ToF calculations
  unsigned long preTransmitTime = millis();
  Serial.print("Pre-transmit time: ");
  Serial.println(preTransmitTime);
  Serial.print("Processing time: ");
  Serial.print(preTransmitTime - processingStartTime);
  Serial.println(" ms");
  Serial.print("Total response time: ");
  Serial.print(preTransmitTime - lastReceiveTime);
  Serial.println(" ms");
  
  // Ensure clean radio state before transmission
  LoRa.idle();
  delay(preTransmitDelay);
  
  // Transmit with precise timing
  LoRa.beginPacket();
  LoRa.print(message);
  int result = LoRa.endPacket(false); // Non-blocking mode
  
  unsigned long postTransmitTime = millis();
  
  if (result == 1) {
    Serial.println("Transmission successful");
  } else {
    Serial.println("Transmission failed!");
  }
  
  Serial.print("Post-transmit time: ");
  Serial.println(postTransmitTime);
  Serial.print("Transmission duration: ");
  Serial.print(postTransmitTime - preTransmitTime);
  Serial.println(" ms");
  
  // Wait for transmission to complete and radio to settle
  delay(radioSettleTime);
  
  // Re-enter receive mode
  enterReceiveMode();
  
  Serial.println("Message sent! Radio back in receive mode...");
  Serial.println();
}

void receiveMessage() {
  int packetSize = LoRa.parsePacket();
  
  if (packetSize) {
    lastReceiveTime = millis();
    processingStartTime = lastReceiveTime;
    
    Serial.println("=== MESSAGE RECEIVED ===");
    Serial.print("Receive time: ");
    Serial.println(lastReceiveTime);
    Serial.print("Packet size: ");
    Serial.println(packetSize);
    Serial.print("Message: ");
    
    String receivedMessage = "";
    while (LoRa.available()) {
      receivedMessage += (char)LoRa.read();
    }
    
    Serial.println(receivedMessage);
    
    // Parse sequence number if it's in expected format
    if (receivedMessage.startsWith("PING:LoRa1:")) {
      int seqStart = receivedMessage.lastIndexOf(':') + 1;
      int receivedSeq = receivedMessage.substring(seqStart).toInt();
      
      if (receivedSeq != expectedSequence) {
        Serial.print("*** SEQUENCE MISMATCH: Expected ");
        Serial.print(expectedSequence);
        Serial.print(", got ");
        Serial.println(receivedSeq);
      } else {
        Serial.println("*** SEQUENCE CORRECT");
      }
      expectedSequence = receivedSeq + 1;
    }
    
    Serial.print("RSSI: ");
    Serial.print(LoRa.packetRssi());
    Serial.println(" dBm");
    Serial.print("SNR: ");
    Serial.print(LoRa.packetSnr());
    Serial.println(" dB");
    Serial.print("Frequency Error: ");
    Serial.print(LoRa.packetFrequencyError());
    Serial.println(" Hz");
    Serial.println("========================");
    
    // Fixed delay before responding (deterministic for ToF)
    Serial.print("Preparing response in ");
    Serial.print(responseDelay);
    Serial.println(" ms...");
    
    delay(responseDelay);
    sendMessage();
  }
}