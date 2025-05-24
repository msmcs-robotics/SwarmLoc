// Adafruit Feather ESP32 with GPS FeatherWing and OLED Display
// Display GPS coordinates in DMS format on OLED

#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_GPS.h>

// OLED Display setup
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA);

// GPS setup
#define GPSSerial Serial1
Adafruit_GPS GPS(&GPSSerial);
#define GPSECHO false

uint32_t timer = millis();
bool gpsDataUpdated = false;

// Function to convert decimal degrees to degrees, minutes, seconds
void convertToDMS(float decimal_degrees, int &degrees, int &minutes, float &seconds) {
  degrees = (int)decimal_degrees;
  float temp = (decimal_degrees - degrees) * 60;
  minutes = (int)temp;
  seconds = (temp - minutes) * 60;
}

// Function to format coordinates as string
String formatCoordinate(float coordinate, char hemisphere) {
  int deg, min;
  float sec;
  convertToDMS(coordinate, deg, min, sec);
  
  // Use asterisk instead of degree symbol to avoid encoding issues
  String result = String(deg) + "*" + String(min) + "'" + String(sec, 1) + "\"" + String(hemisphere);
  return result;
}

// Function to display GPS data on OLED
void updateDisplay() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  // First line: Title
  u8g2.drawStr(0, 10, "GPS Tracker");
  
  if (GPS.fix) {
    // Convert GPS coordinates from DDMM.MMMM to decimal degrees
    float lat_decimal = (int)(GPS.latitude / 100) + ((GPS.latitude - (int)(GPS.latitude / 100) * 100) / 60);
    float lon_decimal = (int)(GPS.longitude / 100) + ((GPS.longitude - (int)(GPS.longitude / 100) * 100) / 60);
    
    // Format coordinates
    String latStr = formatCoordinate(lat_decimal, GPS.lat);
    String lonStr = formatCoordinate(lon_decimal, GPS.lon);
    
    // Second line: Latitude
    u8g2.drawStr(0, 20, "Lat:");
    u8g2.drawStr(25, 20, latStr.c_str());
    
    // Third line: Longitude  
    u8g2.drawStr(0, 30, "Lon:");
    u8g2.drawStr(25, 30, lonStr.c_str());
    
  } else {
    // No GPS fix
    u8g2.drawStr(0, 20, "Searching for");
    u8g2.drawStr(0, 30, "GPS signal...");
  }
  
  u8g2.sendBuffer();
}

// Loading bar function for startup
void showLoadingBar(int percentage) {
  int barWidth = 100;
  int barHeight = 6;
  int filledWidth = map(percentage, 0, 100, 0, barWidth);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.drawStr(0, 10, "Initializing GPS");
  
  // Draw percentage
  u8g2.setCursor(85, 20);
  u8g2.print(percentage);
  u8g2.print("%");

  // Draw loading bar
  u8g2.drawFrame(0, 22, barWidth, barHeight);
  u8g2.drawBox(0, 22, filledWidth, barHeight);

  u8g2.sendBuffer();
}

void setup() {
  // Initialize display
  u8g2.begin();
  
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("GPS OLED Display Starting...");
  
  // Show loading sequence
  for (int i = 0; i <= 100; i += 10) {
    showLoadingBar(i);
    delay(200);
  }
  
  // Initialize GPS
  GPS.begin(9600);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  GPS.sendCommand(PGCMD_ANTENNA);
  
  delay(1000);
  GPSSerial.println(PMTK_Q_RELEASE);
  
  timer = millis();
  gpsDataUpdated = false;
}

void loop() {
  // Read GPS data
  char c = GPS.read();
  if (GPSECHO && c) Serial.print(c);
  
  // Process GPS sentences
  if (GPS.newNMEAreceived()) {
    if (GPSECHO) Serial.print(GPS.lastNMEA());
    if (!GPS.parse(GPS.lastNMEA()))
      return;
  }

  // Print to serial every 2 seconds (for debugging)
  if (millis() - timer > 2000) {
    timer = millis();
    gpsDataUpdated = true; // Flag that new data is available
    
    if (GPS.fix) {
      float lat_decimal = (int)(GPS.latitude / 100) + ((GPS.latitude - (int)(GPS.latitude / 100) * 100) / 60);
      float lon_decimal = (int)(GPS.longitude / 100) + ((GPS.longitude - (int)(GPS.longitude / 100) * 100) / 60);
      
      Serial.println("GPS Coordinates:");
      Serial.println("Lat: " + formatCoordinate(lat_decimal, GPS.lat));
      Serial.println("Lon: " + formatCoordinate(lon_decimal, GPS.lon));
      Serial.println();
    } else {
      Serial.println("No GPS fix available");
      Serial.println();
    }
  }
  
  // Update display immediately when new GPS data is available
  if (gpsDataUpdated) {
    gpsDataUpdated = false; // Reset the flag
    updateDisplay();
  }
}