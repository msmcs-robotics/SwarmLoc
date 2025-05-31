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

// Node identification
const char* nodeID = "LoRa2";
unsigned long responseDelay = 2000; // Wait 2 seconds before responding (longer than Node 1)
int messageCounter = 0;
unsigned long lastReceiveTime = 0;
bool isTransmitting = false;

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial time to initialize
  
  Serial.println("LoRa Node 2 - Enhanced Ping-Pong Responder");
  Serial.println("==========================================");

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

  // Set LoRa parameters (same as Node 1)
  LoRa.setTxPower(20);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  
  // Enable CRC
  LoRa.enableCrc();

  Serial.println("LoRa parameters set:");
  Serial.println("- TX Power: 20 dBm");
  Serial.println("- Spreading Factor: 7");
  Serial.println("- Bandwidth: 125 kHz");
  Serial.println("- Coding Rate: 4/5");
  Serial.println("- CRC: Enabled");
  Serial.println();

  Serial.println("LoRa2 ready! Waiting for messages...");
  Serial.println();
}

void loop() {
  // Only check for messages if we're not in the middle of transmitting
  if (!isTransmitting) {
    receiveMessage();
  }
  
  delay(50); // Smaller delay for better responsiveness
}

void sendMessage() {
  messageCounter++;
  String message = "Hello from LoRa2 #" + String(messageCounter);
  
  isTransmitting = true; // Set flag to prevent receiving during transmission
  
  Serial.println("=== SENDING MESSAGE ===");
  Serial.print("Message: ");
  Serial.println(message);
  Serial.print("Time: ");
  Serial.println(millis());
  Serial.print("Response to message received at: ");
  Serial.println(lastReceiveTime);
  
  // Send LoRa packet
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
  
  Serial.println("Message sent! Waiting for next message...");
  Serial.println();
  
  // Add a small delay after transmission to let the radio settle
  delay(100);
  isTransmitting = false; // Clear flag to resume receiving
}

void receiveMessage() {
  int packetSize = LoRa.parsePacket();
  
  if (packetSize) {
    lastReceiveTime = millis();
    
    Serial.println("=== MESSAGE RECEIVED ===");
    Serial.print("Packet size: ");
    Serial.println(packetSize);
    Serial.print("Message: ");
    
    String receivedMessage = "";
    while (LoRa.available()) {
      receivedMessage += (char)LoRa.read();
    }
    
    Serial.println(receivedMessage);
    Serial.print("RSSI: ");
    Serial.print(LoRa.packetRssi());
    Serial.println(" dBm");
    Serial.print("SNR: ");
    Serial.print(LoRa.packetSnr());
    Serial.println(" dB");
    Serial.print("Frequency Error: ");
    Serial.print(LoRa.packetFrequencyError());
    Serial.println(" Hz");
    Serial.print("Time: ");
    Serial.println(millis());
    Serial.println("========================");
    
    // Wait before responding to avoid collisions
    Serial.print("Preparing response in ");
    Serial.print(responseDelay);
    Serial.println(" ms...");
    
    delay(responseDelay);
    sendMessage();
  }
}