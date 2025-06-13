/*
 * DWS1000 UWB Distance Measurement - INITIATOR
 * This device sends ranging requests and calculates distance
 */

 #include <SPI.h>
 #include <DW1000.h>
 
 // Connection pins
 const uint8_t PIN_RST = 9;
 const uint8_t PIN_IRQ = 2;
 const uint8_t PIN_SS = SS;
 
 // Variables for ranging
 volatile boolean sentAck = false;
 volatile boolean receivedAck = false;
 volatile boolean error = false;
 volatile int16_t numReceived = 0;
 String message;
 
 // Time stamps
 uint64_t timePollSent;
 uint64_t timePollReceived;
 uint64_t timePollAckSent;
 uint64_t timePollAckReceived;
 uint64_t timeRangeSent;
 uint64_t timeRangeReceived;
 
 // Device addresses
 byte target_address[] = {0x02, 0x00};
 byte source_address[] = {0x01, 0x00};
 
 void setup() {
   Serial.begin(9600);
   delay(1000);
   Serial.println("### DWS1000 Initiator ###");
   
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
   DW1000.setDeviceAddress(1);
   DW1000.setNetworkId(10);
   DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
   DW1000.commitConfiguration();
   
   Serial.println("Configuration complete");
   
   // Attach interrupt
   attachInterrupt(digitalPinToInterrupt(PIN_IRQ), handleInterrupt, RISING);
   
   // Start ranging
   startRanging();
 }
 
 void loop() {
   if (receivedAck) {
     receivedAck = false;
     
     // Calculate distance
     float distance = calculateDistance();
     
     Serial.print("Distance: ");
     Serial.print(distance);
     Serial.println(" m");
     
     // Wait before next ranging
     delay(1000);
     
     // Start next ranging cycle
     startRanging();
   }
   
   if (error) {
     error = false;
     Serial.println("Error in ranging");
     delay(1000);
     startRanging();
   }
 }
 
 void handleInterrupt() {
   // Handle DW1000 interrupt
   DW1000.handleInterrupt();
 }
 
 void startRanging() {
   // Send poll message
   DW1000.newTransmit();
   DW1000.setDefaults();
   
   byte pollMsg[] = {0x61, 0x88, 0, 0xCA, 0xDE, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00};
   DW1000.setData(pollMsg, sizeof(pollMsg));
   
   DW1000.startTransmit();
   timePollSent = DW1000.getTransmitTimestamp();
 }
 
 float calculateDistance() {
   // TWR (Two Way Ranging) calculation
   // This is a simplified version - real implementation needs more precise timing
   
   int64_t round1 = (timePollAckReceived - timePollSent);
   int64_t reply1 = (timePollAckSent - timePollReceived);
   int64_t round2 = (timeRangeReceived - timePollAckSent);
   int64_t reply2 = (timeRangeSent - timePollAckReceived);
   
   float clockOffsetRatio = (float)reply1 / (float)reply2;
   int64_t tof = (round1 * round2 - reply1 * reply2) / (round1 + round2 + reply1 + reply2);
   
   float distance = (float)tof * 0.0154689; // Convert to meters
   
   return distance;
 }