/*
LoRa TX → ESP32 RX (e.g., GPIO 16)

LoRa RX → ESP32 TX (e.g., GPIO 17)

VCC → 3.3V

GND → GND
 */

#include <HardwareSerial.h>

HardwareSerial loraSerial(1); // Use UART1
#define LORA_RX 16
#define LORA_TX 17

long baudRates[] = {9600, 115200, 57600, 38400, 19200, 4800};

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Scanning baud rates...");

  for (int i = 0; i < sizeof(baudRates) / sizeof(baudRates[0]); i++) {
    long baud = baudRates[i];
    loraSerial.begin(baud, SERIAL_8N1, LORA_RX, LORA_TX);
    delay(100);
    loraSerial.println("AT");
    delay(500);

    String response = "";
    while (loraSerial.available()) {
      response += char(loraSerial.read());
    }

    Serial.print("Tried baud ");
    Serial.print(baud);
    Serial.print(" - Response: ");
    Serial.println(response);

    if (response.indexOf("+OK") >= 0) {
      Serial.println("✅ Found correct baud rate: " + String(baud));
      break;
    }
  }
}

void loop() {}