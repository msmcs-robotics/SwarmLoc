/*
 * LoRa Node 1 - Adafruit Huzzah32 + RFM95W FeatherWing
 * Initiates communication, then responds when receiving messages
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
 * 
 * Might need to press reset button once after upload to make work
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
const char* nodeID = "LoRa1";
bool hasStarted = false;
unsigned long responseDelay = 1000;
unsigned long lastSendTime = 0;
unsigned long sendInterval = 10000; // Send every 10 seconds if no response
int messageCounter = 0;
bool waitingForResponse = false;

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial time to initialize
  
  Serial.println("LoRa Node 1 - Enhanced Ping-Pong Initiator");
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

  // Set LoRa parameters
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

  Serial.println("LoRa1 ready! Will start communication in 5 seconds...");
  Serial.println("Make sure LoRa2 is running and ready!");
  
  delay(5000); // Give more time for the other node to be ready
  sendMessage();
  hasStarted = true;
}

void loop() {
  receiveMessage();
  
  // If we haven't received a response in a while, send another message
  if (waitingForResponse && (millis() - lastSendTime > sendInterval)) {
    Serial.println("No response received, sending again...");
    sendMessage();
  }
  
  delay(100);
}

void sendMessage() {
  messageCounter++;
  String message = "Hello from LoRa1 #" + String(messageCounter);
  
  Serial.println("=== SENDING MESSAGE ===");
  Serial.print("Message: ");
  Serial.println(message);
  Serial.print("Time: ");
  Serial.println(millis());
  
  // Send LoRa packet
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
  
  lastSendTime = millis();
  waitingForResponse = true;
  
  Serial.println("Message sent! Waiting for response...");
  Serial.println();
}

void receiveMessage() {
  int packetSize = LoRa.parsePacket();
  
  if (packetSize) {
    waitingForResponse = false; // Got a response!
    
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
    
    // Wait then respond
    Serial.println("Preparing response...");
    delay(responseDelay);
    sendMessage();
  }
}
