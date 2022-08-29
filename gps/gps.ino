#include<SoftwareSerial.h>    
#include<TinyGPS.h>           

SoftwareSerial SerialGPS(8, 9);     
TinyGPS GPS;                       

float lat, lon, vel;
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

      vel = GPS.f_speed_kmph();

      dtostrf(lat, 2, 6, lat_str);
      dtostrf(lon, 2, 6, lon_str);
      dtostrf(vel, 3, 2, speed_str);
      sprintf(data, "{\"ts\": %s, \"lat\": %s, \"lon\": %s, \"speed\": %s}", ts, lat_str, lon_str, speed_str);
      Serial.println(data);
            

      //Satelites
      sat = GPS.satellites();

      if (sat != TinyGPS::GPS_INVALID_SATELLITES) {
        Serial.print("Satelites: ");
        Serial.println(sat);
      }

      Serial.println("");
    }
  }
}
