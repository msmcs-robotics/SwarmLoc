/*
 * Arduino Uno LoRa Communication with REYAX RYLR890 - SIMPLE TRANSMITTER
 * LoRa TX → Uno Pin 2 (SoftwareSerial RX)
 * LoRa RX → Uno Pin 3 (SoftwareSerial TX)
 */

#include <SoftwareSerial.h>

SoftwareSerial loraSerial(2, 3); // RX, TX
int counter = 0;

void setup() {
  Serial.begin(9600);
  loraSerial.begin(115200);
  
  Serial.println("Arduino LoRa Transmitter");
  delay(2000);
  
  // Simple setup
  loraSerial.println("AT+ADDRESS=0");
  delay(100);
  loraSerial.println("AT+NETWORKID=3");
  delay(100);
  
  Serial.println("Ready to transmit!");
}

void loop() {
  counter++;
  String message = "Hello #" + String(counter);
  
  Serial.print("Sending: ");
  Serial.println(message);
  
  // Send message to address 1 (ESP32)
  loraSerial.print("AT+SEND=1,");
  loraSerial.print(message.length());
  loraSerial.print(",");
  loraSerial.println(message);
  
  // Check response
  delay(500);
  if (loraSerial.available()) {
    String response = loraSerial.readString();
    Serial.print("Response: ");
    Serial.println(response);
  }
  
  delay(3000); // Send every 3 seconds
}
