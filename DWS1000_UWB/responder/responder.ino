/*
 * DWS1000 UWB Distance Measurement - RESPONDER
 * This device responds to ranging requests
 */

 #include <SPI.h>
 #include <DW1000.h>
 
 // Connection pins
 const uint8_t PIN_RST = 9;
 const uint8_t PIN_IRQ = 2;
 const uint8_t PIN_SS = SS;
 
 // Variables for ranging
 volatile boolean receivedPoll = false;
 volatile boolean error = false;
 String message;
 
 // Device addresses
 byte target_address[] = {0x01, 0x00};
 byte source_address[] = {0x02, 0x00};
 
 void setup() {
   Serial.begin(9600);
   delay(1000);
   Serial.println("### DWS1000 Responder ###");
   
   // Initialize the pins
   pinMode(PIN_RST, OUTPUT);
   digitalWrite(PIN_RST, HIGH);
   delay(100);
   digitalWrite(PIN_RST, LOW);
   delay(100);
   digitalWrite(PIN_RST, HIGH);
   delay(100);
   
   // Initialize DW1000
   DW1000.begin(PIN_IRQ, PIN_RST);
   DW1000.select(PIN_SS);
   
   Serial.println("DW1000 initialized ...");
   
   // General configuration
   DW1000.newConfiguration();
   DW1000.setDefaults();
   DW1000.setDeviceAddress(2);
   DW1000.setNetworkId(10);
   DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
   DW1000.commitConfiguration();
   
   Serial.println("Configuration complete");
   
   // Attach interrupt
   attachInterrupt(digitalPinToInterrupt(PIN_IRQ), handleInterrupt, RISING);
   
   // Start listening
   startListening();
 }
 
 void loop() {
   if (receivedPoll) {
     receivedPoll = false;
     
     Serial.println("Poll received, sending response");
     
     // Send poll acknowledgment
     sendPollAck();
     
     // Continue listening
     delay(100);
     startListening();
   }
   
   if (error) {
     error = false;
     Serial.println("Error occurred");
     delay(100);
     startListening();
   }
 }
 
 void handleInterrupt() {
   // Check if we received a message
   if (DW1000.isReceiveDone()) {
     receivedPoll = true;
     DW1000.clearReceiveStatus();
   } else if (DW1000.isTransmitDone()) {
     DW1000.clearTransmitStatus();
   } else {
     error = true;
     DW1000.clearAllStatus();
   }
 }
 
 void startListening() {
   DW1000.newReceive();
   DW1000.setDefaults();
   DW1000.receivePermanently(true);
   DW1000.startReceive();
 }
 
 void sendPollAck() {
   DW1000.newTransmit();
   DW1000.setDefaults();
   
   byte pollAckMsg[] = {0x61, 0x88, 0, 0xCA, 0xDE, 0x02, 0x00, 0x01, 0x00, 0x01, 0x00};
   DW1000.setData(pollAckMsg, sizeof(pollAckMsg));
   
   DW1000.startTransmit();
 }