// adafruit feather esp32 and adafruit GPS featherwing
// pretty print the GPS coordinates

// boards > esp32 > adafruit esp32 feather

#include <Adafruit_GPS.h>

// what's the name of the hardware serial port?
#define GPSSerial Serial1

// Connect to the GPS on the hardware port
Adafruit_GPS GPS(&GPSSerial);

// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences
#define GPSECHO false

uint32_t timer = millis();

// Function to convert decimal degrees to degrees, minutes, seconds
void convertToDMS(float decimal_degrees, int &degrees, int &minutes, float &seconds) {
  degrees = (int)decimal_degrees;
  float temp = (decimal_degrees - degrees) * 60;
  minutes = (int)temp;
  seconds = (temp - minutes) * 60;
}

// Function to format and print coordinates
void printFormattedCoordinates() {
  if (GPS.fix) {
    // Convert GPS latitude from DDMM.MMMM to decimal degrees
    float lat_decimal = (int)(GPS.latitude / 100) + ((GPS.latitude - (int)(GPS.latitude / 100) * 100) / 60);
    
    // Convert GPS longitude from DDDMM.MMMM to decimal degrees  
    float lon_decimal = (int)(GPS.longitude / 100) + ((GPS.longitude - (int)(GPS.longitude / 100) * 100) / 60);
    
    // Convert to degrees, minutes, seconds
    int lat_deg, lat_min, lon_deg, lon_min;
    float lat_sec, lon_sec;
    
    convertToDMS(lat_decimal, lat_deg, lat_min, lat_sec);
    convertToDMS(lon_decimal, lon_deg, lon_min, lon_sec);
    
    // Print formatted coordinates
    Serial.println("Coordinates:");
    Serial.print(lat_deg);
    Serial.print("°");
    Serial.print(lat_min);
    Serial.print("'");
    Serial.print(lat_sec, 1);
    Serial.print("\"");
    Serial.print(GPS.lat);
    Serial.print(" ");
    Serial.print(lon_deg);
    Serial.print("°");
    Serial.print(lon_min);
    Serial.print("'");
    Serial.print(lon_sec, 1);
    Serial.print("\"");
    Serial.println(GPS.lon);
    Serial.println();
  } else {
    Serial.println("No GPS fix available");
    Serial.println();
  }
}

void setup()
{
  //while (!Serial);  // uncomment to have the sketch wait until Serial is ready

  // connect at 115200 so we can read the GPS fast enough and echo without dropping chars
  // also spit it out
  Serial.begin(115200);
  Serial.println("Adafruit GPS library basic parsing test!");

  // 9600 NMEA is the default baud rate for Adafruit MTK GPS's- some use 4800
  GPS.begin(9600);
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // For parsing data, we don't suggest using anything but either RMC only or RMC+GGA since
  // the parser doesn't care about other sentences at this time
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // 1 Hz update rate
  // For the parsing code to work nicely and have time to sort thru the data, and
  // print it out we don't suggest using anything higher than 1 Hz

  // Request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);

  delay(1000);

  // Ask for firmware version
  GPSSerial.println(PMTK_Q_RELEASE);
}

void loop() // run over and over again
{
  // read data from the GPS in the 'main loop'
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
  if (GPSECHO)
    if (c) Serial.print(c);
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences!
    // so be very wary if using OUTPUT_ALLDATA and trying to print out data
    if (GPSECHO) Serial.print(GPS.lastNMEA()); // this also sets the newNMEAreceived() flag to false
    if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
      return; // we can fail to parse a sentence in which case we should just wait for another
  }

  // approximately every 2 seconds or so, print out the current stats
  if (millis() - timer > 2000) {
    timer = millis(); // reset the timer
    
    // Print only the formatted coordinates
    printFormattedCoordinates();
  }
}