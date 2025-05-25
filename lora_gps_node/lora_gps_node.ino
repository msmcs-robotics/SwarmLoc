/*
 * ESP32 Feather LoRa Communication with REYAX RYLR890 - SIMPLE RECEIVER
 * LoRa TX → ESP32 RX (GPIO16)
 * LoRa RX → ESP32 TX (GPIO17)
 */

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // RX=16, TX=17
  
  Serial.println("ESP32 LoRa Receiver");
  delay(2000);
  
  // Simple setup
  Serial2.println("AT+ADDRESS=1");
  delay(100);
  Serial2.println("AT+NETWORKID=3");
  delay(100);
  
  Serial.println("Ready to receive!");
}

void loop() {
  // Check for incoming messages
  if (Serial2.available()) {
    String received = Serial2.readString();
    received.trim();
    
    Serial.print("Received: ");
    Serial.println(received);
    
    // Parse if it's a message (+RCV format)
    if (received.startsWith("+RCV=")) {
      int start = received.indexOf(',', received.indexOf(',') + 1) + 1;
      int end = received.indexOf(',', start);
      if (end == -1) end = received.length();
      
      String message = received.substring(start, end);
      Serial.print("Message: ");
      Serial.println(message);
      Serial.println("---");
    }
  }
  
  delay(100);
}
