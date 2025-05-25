#include <SoftwareSerial.h>

int rxPin = 2;
int txPin = 3;

SoftwareSerial loraSerial(rxPin, txPin); // RX=2, TX=3

int baudRates[] = {9600, 19200, 38400, 57600, 115200};
int numRates = 5;
int currentRate = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("LoRa Baud Rate Scanner");
  Serial.println("Trying different baud rates...");
  testNextBaudRate();
}

void loop() {
  if (loraSerial.available()) {
    String response = loraSerial.readString();
    Serial.print("Response at ");
    Serial.print(baudRates[currentRate]);
    Serial.print(" baud: ");
    Serial.println(response);
    
    // Check if response looks valid (contains readable characters)
    if (response.indexOf("VER") >= 0 || response.indexOf("+") >= 0) {
      Serial.print("*** SUCCESS! Use baud rate: ");
      Serial.println(baudRates[currentRate]);
      while(1); // Stop here
    }
  }
  
  // Wait 3 seconds then try next baud rate
  static unsigned long lastTry = 0;
  if (millis() - lastTry > 3000) {
    lastTry = millis();
    currentRate++;
    if (currentRate >= numRates) {
      currentRate = 0;
      Serial.println("Completed one cycle, starting over...");
    }
    testNextBaudRate();
  }
}

void testNextBaudRate() {
  Serial.print("Testing baud rate: ");
  Serial.println(baudRates[currentRate]);
  
  loraSerial.end();
  loraSerial.begin(baudRates[currentRate]);
  delay(100);
  
  loraSerial.println("AT+VER?");
}
