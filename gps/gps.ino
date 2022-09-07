#include<SoftwareSerial.h>
#include<TinyGPS.h>
#include <SD.h>

// CMD to allow monitor serial in linux
// sudo chmod a+rw /dev/ttyACM0

SoftwareSerial SerialGPS(8, 9);
TinyGPS GPS;


File gps_data_file;
char filename[16];


float lat, lon;
char lat_str[10], lon_str[10];
unsigned long date, hour;
char d_str[8], h_str[8];
unsigned short sat;
char ts[32];
char data[70]; // data to be stored
char speed_str[8];


void format_datetime() {
  // day begins with 0
  if (date < 100000) {
    sprintf(d_str, "0%ld", date);
  } else {
    sprintf(d_str, "%ld", date);
  }

  // hour begins with 0
  if (hour < 10000000) {
    sprintf(h_str, "0%ld", hour);
  } else {
    sprintf(h_str, "%ld", hour);
  }
}

void setup() {

  SerialGPS.begin(9600);
  Serial.begin(9600);

  Serial.print("Initializing SD card...");
  if (!SD.begin(10)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("SD initialization done.");
  
  Serial.println("Buscando satelites...");
}

void loop() {

  while (SerialGPS.available()) {
    if (GPS.encode(SerialGPS.read())) {
      GPS.get_datetime(&date, &hour);

      format_datetime();
      
      // build ts
      sprintf(ts, "20%c%c-%c%c-%c%c %c%c:%c%c:%c%c",
        d_str[4], d_str[5], // year
        d_str[2], d_str[3], // month
        d_str[0], d_str[1], // day
        h_str[0], h_str[1], // hour
        h_str[2], h_str[3], // minute
        h_str[4], h_str[5]  // seconds
      );

      GPS.f_get_position(&lat, &lon);


      dtostrf(lat, 2, 6, lat_str);
      dtostrf(lon, 2, 6, lon_str);
      dtostrf(GPS.f_speed_kmph(), 3, 2, speed_str);
      
      // buid data
      sprintf(data, "{\"ts\": %s, \"lat\": %s, \"lon\": %s, \"speed\": %s}", ts, lat_str, lon_str, speed_str);
      
      // writing to file
      sprintf(filename, "%s%c%c.txt", d_str, h_str[0], h_str[1]);

      gps_data_file = SD.open(filename, FILE_WRITE);
      if (gps_data_file) {
        Serial.print("Writing data to file...");
        gps_data_file.println(data);
        gps_data_file.close();
        Serial.println("Done.");
      } else {
        Serial.println("error opening data file");
      }
                  

      //Satelites
      sat = GPS.satellites();

      if (sat != TinyGPS::GPS_INVALID_SATELLITES) {
        Serial.print("Satelites: ");
        Serial.println(sat);
      }

      delay(9000); // aprox 10 sec for each gps reading
    }
  }
}
