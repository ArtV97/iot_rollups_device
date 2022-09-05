#include<SoftwareSerial.h>
#include<TinyGPS.h>
#include <SD.h>

// CMD allow monitor serial in linux
// sudo chmod a+rw /dev/ttyACM0

SoftwareSerial SerialGPS(8, 9);
TinyGPS GPS;
File gps_data_file;


float lat, lon;//, speed;
unsigned long date, hour;
unsigned short sat;
char ts[32]; // data to be stored
char data[200];
char lat_str[12];
char lon_str[12];
char speed_str[8];

void setup() {

  SerialGPS.begin(9600);
  Serial.begin(9600);

  Serial.print("Initializing SD card...");
  if (!SD.begin(10)) {
    Serial.println("initialization failed!");
    while (1);
  }

  Serial.println("SD initialization done.");
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
//  gps_data_file = SD.open("gps_data.txt", FILE_WRITE);

  // if the file opened okay, write to it:
//  if (gps_data_file) {
//    Serial.print("Writing to test.txt...");
//    gps_data_file.println("This is a test file :)");
//    gps_data_file.println("testing 1, 2, 3.");
//    for (int i = 0; i < 20; i++) {
//      gps_data_file.println(i);
//    }
//    // close the file:
//    gps_data_file.close();
//    Serial.println("done.");
//  }
//  else {
//    // if the file didn't open, print an error:
//    Serial.println("error opening test.txt");
//    while(1);
//  }

  Serial.println("Buscando satelites...");
}

void loop() {

  while (SerialGPS.available()) {
    if (GPS.encode(SerialGPS.read())) {
      GPS.get_datetime(&date, &hour);

      // Serial.print(date);
      // Serial.print(", ");
      // Serial.println(hour);
      sprintf(ts, "%ld/%ld/%ld %ld:%ld:%ld", date / 10000, (date % 10000) / 100, date % 100, hour / 1000000, (hour % 1000000) / 10000, (hour % 10000) / 100);       
      //Serial.println(ts);

      GPS.f_get_position(&lat, &lon);

      //speed = GPS.f_speed_kmph();

      dtostrf(lat, 2, 6, lat_str);
      dtostrf(lon, 2, 6, lon_str);
      dtostrf(GPS.f_speed_kmph(), 3, 2, speed_str);
      sprintf(data, "{\"ts\": %s, \"lat\": %s, \"lon\": %s, \"speed\": %s}", ts, lat_str, lon_str, speed_str);
      //Serial.println(data);
      
      // writing to file
      gps_data_file = SD.open("gps_data.txt", FILE_WRITE);
      if (gps_data_file) {
        Serial.print("Writing data to file...");
        gps_data_file.println(data);
        gps_data_file.close();
        Serial.println("Done.");
      }
      else {
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
