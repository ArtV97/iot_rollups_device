#include<SoftwareSerial.h>
#include<TinyGPS.h>
#include <SD.h>

#define SEND_AFTER 2 // send data after 10 reads

// CMD to allow monitor serial in linux
// sudo chmod a+rw /dev/ttyACM0

SoftwareSerial sim800l(2,3); // (TX,RX)
SoftwareSerial SerialGPS(8,9);  // (TX,RX)

TinyGPS GPS;
File gps_data_file;
char filename[16];
uint8_t data_count = 0;

char data[70]; // data to be stored


void format_datetime(unsigned long date, unsigned long hour, char *d_str, char *h_str) {
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


void fgets_sim800l(char *buff, int buff_sz) {
  if (sim800l.available()) {
    int availableBytes = sim800l.available();

    uint8_t line_counter = 0;
    uint8_t j = 0;
    for (int i = 0; i < availableBytes-1; i++) {
      char c = sim800l.read();
      if (c == '\n' || c == '\r') {
        line_counter++;
        continue;
      }
      if (line_counter < 2) continue;
      buff[j] = c;
      j++;
      
      if (j > buff_sz-1) {
        return;
      }
    }
    buff[j] = '\0';
  }
}


bool sim800l_ready() {
  sim800l.listen();
  char buff[32];
  char *p;
  
  // AT commands reconized
  Serial.println("AT");
  sim800l.println(F("AT"));
  delay(2000);
  fgets_sim800l(buff, 32);
  p = buff;
  Serial.println(p);
  if (strcmp(p, "OK")) return false; // fail

  // is sim card ok
  Serial.println("AT+CPIN?");
  sim800l.println(F("AT+CPIN?"));
  delay(1500);
  fgets_sim800l(buff, 32);
  p = buff+7;
  if (strcmp(p, "READYOK")) return false; // fail

  
  // Signal Quality
  Serial.println("AT+CSQ");
  sim800l.println(F("AT+CSQ"));
  delay(2000);
  fgets_sim800l(buff, 32);
  p = buff+6;
  if (p[0] == '0') return false; // fail(No signal)
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // Display the current network operator currently registered.
  Serial.println("AT+COPS?");
  sim800l.println(F("AT+COPS?"));
  delay(3000);
  fgets_sim800l(buff, 32);
  
  if (strcmp(buff, "+COPS: 0OK") == 0) return false; // don't have a operator
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  p = buff+11;
  Serial.print("Conected to operator: ");
  do {
    Serial.print(p[0]);
    p++;
  } while (p[0] != '"');
  Serial.println(p[0]);


  return true;
}



bool send_data(char *data) {
  
  Serial.println("SEND DATA");
  char buff[32];
  char *p;
  // set bearer parameters (Type of Internet connection, "GPRS" or "CSD")
  sim800l.println(F("AT+SAPBR=3,1,\"Contype\",\"GPRS\""));
  delay(5000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail


  // set bearer parameters (access point name)
  sim800l.println(F("AT+SAPBR=3,1,\"APN\",\"java.claro.com.br\""));
  delay(6000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail
 
  // open bearer
  sim800l.println(F("AT+SAPBR=1,1"));
  delay(10000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // query bearer
  sim800l.println(F("AT+SAPBR=2,1"));
  delay(6000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // Initialize HTTP service
  sim800l.println(F("AT+HTTPINIT"));
  delay(5000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // Set HTTP parameters value (Bearer profile identifier) * Mandatory parameter
  sim800l.println(F("AT+HTTPPARA=\"CID\",1"));
  delay(6000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // Server address
  sim800l.println(F("AT+HTTPPARA=\"URL\",\"http://200.19.119.123:3000\""));
  delay(4000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // Set the “Content-Type” field in HTTP header
  sim800l.println(F("AT+HTTPPARA=\"CONTENT\",\"application/json\""));
  delay(4000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  
  // Input HTTP data (Sending data size and time limit(ms))
  sim800l.print(F("AT+HTTPDATA="));
  sim800l.print(strlen(data));
  sim800l.println(",100000");
  delay(6000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail


  // Sending data
  sim800l.println(data);
  Serial.print("Sending -> ");
  Serial.print(data);
  Serial.println("...");
  delay(6000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // HTTP method action (0 = GET, 1 = POST)
  sim800l.println(F("AT+HTTPACTION=1"));
  delay(6000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // Read the HTTP server response
  sim800l.println(F("AT+HTTPREAD"));
  delay(6000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // Terminate HTTP service
  sim800l.println(F("AT+HTTPTERM"));
  delay(10000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  sim800l.println(F("AT+CIPSHUT"));
  delay(1000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail
  
  return 0;

}


void setup() {

  sim800l.begin(9600);
  SerialGPS.begin(9600);
  Serial.begin(9600);

  Serial.print(F("Initializing SD card..."));
  if (!SD.begin(10)) {
    Serial.println(F("initialization failed!"));
    while (1);
  }
  Serial.println(F("SD initialization done."));
}


void loop() {
  SerialGPS.listen();

  while (SerialGPS.available()) {
    if (GPS.encode(SerialGPS.read())) {
      float lat, lon;
      char lat_str[10], lon_str[10];
      unsigned long date, hour;
      char d_str[8], h_str[8];
      char speed_str[8];
      //unsigned short sat;
      char ts[32];

      GPS.get_datetime(&date, &hour);

      format_datetime(date, hour, d_str, h_str);
      
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
      sprintf(data, "{\"ts\": %s,\"lat\": %s,\"lon\": %s,\"speed\": %s}", ts, lat_str, lon_str, speed_str);
      Serial.println(data);
      
      // writing to file
      sprintf(filename, "%s%c%c.txt", d_str, h_str[0], h_str[1]);

      gps_data_file = SD.open(filename, FILE_WRITE);
      if (gps_data_file) {
        Serial.print(F("Writing data to file..."));
        gps_data_file.println(data);
        gps_data_file.close();
        Serial.println("Done.");
        data_count++;
      } else {
        Serial.println(F("error opening data file"));
      }


      //Satelites
//      sat = GPS.satellites();
//
//      if (sat != TinyGPS::GPS_INVALID_SATELLITES) {
//        Serial.print("Satelites: ");
//        Serial.println(sat);
//      }

      
      if (data_count == SEND_AFTER) {
        if (sim800l_ready()) {
          send_data(data);
        }
        else {
          Serial.println(F("Fail to send via GSM"));
        }
        
        data_count = 0; // reset
      }
      delay(9000); // aprox 10 sec for each gps reading
    }
  }
}
