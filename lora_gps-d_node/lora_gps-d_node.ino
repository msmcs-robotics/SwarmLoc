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

// LoRa frequency - use appropriate frequency for your region
// 915E6 for North America, 868E6 for Europe, 433E6 for Asia
#define LORA_FREQ   915E6

// Node identification
const char* nodeID = "LoRa2";
unsigned long responseDelay = 1500; // Wait 1.5 seconds before responding (different from Node 1)

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial);
  
  Serial.println("LoRa Node 2 - Ping-Pong Responder");
  Serial.println("----------------------------------");

  // Setup LoRa transceiver module
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

  // Initialize LoRa
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("Starting LoRa failed!");
    Serial.println("Check wiring and connections!");
    while (1);
  }
  
  Serial.println("LoRa init succeeded.");
  Serial.print("Frequency: ");
  Serial.print(LORA_FREQ / 1E6);
  Serial.println(" MHz");

  // Set LoRa parameters for better performance
  LoRa.setTxPower(20);          // Set power to 20 dBm (max for this library)
  LoRa.setSpreadingFactor(7);   // Set spreading factor (6-12)
  LoRa.setSignalBandwidth(125E3); // Set bandwidth
  LoRa.setCodingRate4(5);       // Set coding rate (5-8)
  LoRa.setSyncWord(0x12);       // Set sync word (0x12 is default)

  Serial.println("LoRa2 ready! Waiting for messages...");
  Serial.println();
}

void loop() {
  // Check for incoming messages
  receiveMessage();
  
  delay(100); // Small delay to prevent overwhelming the loop
}

void sendMessage() {
  String message = "Hello from LoRa2";
  
  Serial.print("Sending: ");
  Serial.println(message);
  
  // Send LoRa packet
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
  
  Serial.println("Message sent!");
  Serial.println("Waiting for next message...");
  Serial.println();
}

void receiveMessage() {
  // Try to parse packet
  int packetSize = LoRa.parsePacket();
  
  if (packetSize) {
    // Received a packet
    Serial.println("--- Message Received ---");
    Serial.print("Message: ");
    
    // Read packet
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
    Serial.println("----------------------");
    
    // Wait a bit then respond
    Serial.println("Preparing response...");
    delay(responseDelay);
    sendMessage();
  }
}
