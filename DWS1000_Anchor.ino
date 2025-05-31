// Anchor - Initiator
#include <DW1000.h>

#define PIN_RST 9
#define PIN_IRQ 2
#define PIN_SS 10

void setup() {
  Serial.begin(115200);
  delay(1000);

  DW1000.begin(PIN_IRQ, PIN_RST);
  DW1000.select(PIN_SS);
  DW1000.newConfiguration();
  DW1000.setDeviceAddress(1);
  DW1000.setNetworkId(10);
  DW1000.commitConfiguration();
  Serial.println("Anchor started");
}

void loop() {
  DW1000.newTransmit();
  DW1000.setDefaults();
  DW1000.setData((byte *)"POLL", 4);
  DW1000.startTransmit();

  while (!DW1000.isTransmitDone());

  DW1000.newReceive();
  DW1000.setDefaults();
  DW1000.startReceive();

  unsigned long startTime = micros();
  unsigned long timeout = startTime + 50000; // 50 ms timeout

  while (!DW1000.isReceiveDone()) {
    if (micros() > timeout) {
      Serial.println("Timeout waiting for response");
      return;
    }
  }

  byte data[30];
  DW1000.getData(data, 30);
  if (strncmp((char *)data, "RESP", 4) == 0) {
    long tof = DW1000.getTransmitTimestamp() - DW1000.getReceiveTimestamp();
    Serial.print("ToF (raw): ");
    Serial.println(tof);
  }
}
