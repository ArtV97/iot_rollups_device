#include<SoftwareSerial.h>
#include<TinyGPS.h>
#include <SD.h>

#define SEND_AFTER 1 // send data after 10 reads

// CMD to allow monitor serial in linux
// sudo chmod a+rw /dev/ttyACM0

SoftwareSerial sim800l(2,3); // (TX,RX)
SoftwareSerial SerialGPS(8,9);  // (TX,RX)

// File variables
File gps_data_file;
char filename[16];
uint8_t data_count = 0;

// Sim800L variables
char apn[20] = "";
char url[30] = "";
char port[6] = "";

// GPS Processing variables
TinyGPS GPS;

char bus_line[12] = "";
char data[100]; // data to be stored


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
  Serial.println(buff);
  p = buff+7;
  if (strcmp(p, "READYOK")) return false; // fail

  
  // Signal Quality
  Serial.println("AT+CSQ");
  sim800l.println(F("AT+CSQ"));
  delay(2000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+6;
  if (p[0] == '0') return false; // fail(No signal)
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // Display the current network operator currently registered.
  Serial.println("AT+COPS?");
  sim800l.println(F("AT+COPS?"));
  delay(3000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  
  if (strcmp(buff, "+COPS: 0OK") == 0) return false; // don't have an operator
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
  char buff[32];
  char *p;

  // set bearer parameters (Type of Internet connection, "GPRS" or "CSD")
  Serial.println(F("AT+SAPBR=3,1,\"Contype\",\"GPRS\""));
  sim800l.println(F("AT+SAPBR=3,1,\"Contype\",\"GPRS\""));
  delay(5000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail


  // set bearer parameters (access point name)
  Serial.print(F("AT+SAPBR=3,1,\"APN\",\""));
  Serial.print(apn);
  Serial.println(F("\""));
  sim800l.print(F("AT+SAPBR=3,1,\"APN\",\""));
  sim800l.print(apn);
  sim800l.println(F("\""));
  delay(6000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // open bearer
  Serial.println(F("AT+SAPBR=1,1"));
  sim800l.println(F("AT+SAPBR=1,1"));
  delay(15000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) {
    // close bearer
    Serial.println(F("AT+SAPBR=0,1"));
    sim800l.println(F("AT+SAPBR=0,1"));
    delay(10000);
    fgets_sim800l(buff, 32);
    Serial.println(buff);

    // try again
    Serial.println(F("AGAIN: AT+SAPBR=1,1"));
    sim800l.println(F("AT+SAPBR=1,1"));
    delay(10000);
    fgets_sim800l(buff, 32);
    Serial.println(buff);
    p = buff+strlen(buff)-2;
    if (strcmp(p, "OK")) return false; // fail
  }

  // query bearer
  Serial.println(F("AT+SAPBR=2,1"));
  sim800l.println(F("AT+SAPBR=2,1"));
  delay(6000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // Initialize HTTP service
  Serial.println(F("AT+HTTPINIT"));
  sim800l.println(F("AT+HTTPINIT"));
  delay(5000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail
  
  // Set HTTP parameters value (Bearer profile identifier) * Mandatory parameter
  Serial.println(F("AT+HTTPPARA=\"CID\",1"));
  sim800l.println(F("AT+HTTPPARA=\"CID\",1"));
  delay(6000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // Server address
  Serial.print(F("AT+HTTPPARA=\"URL\",\"http://"));
  Serial.print(url);
  Serial.print(F(":"));
  Serial.print(port);
  Serial.println(F("\""));
  sim800l.print(F("AT+HTTPPARA=\"URL\",\"http://"));
  sim800l.print(url);
  sim800l.print(F(":"));
  sim800l.print(port);
  sim800l.println(F("\""));
  delay(4000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // Set the “Content-Type” field in HTTP header
  Serial.println(F("AT+HTTPPARA=\"CONTENT\",\"application/json\""));
  sim800l.println(F("AT+HTTPPARA=\"CONTENT\",\"application/json\""));
  delay(4000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  
  // Input HTTP data (Sending data size and time limit(ms))
  Serial.print(F("AT+HTTPDATA="));
  Serial.print(strlen(data));
  Serial.println(",100000");
  sim800l.print(F("AT+HTTPDATA="));
  sim800l.print(strlen(data));
  sim800l.println(",100000");
  delay(6000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  

  // Sending data
  sim800l.println(data);
  Serial.print("Sending -> ");
  Serial.println(data);
  delay(6000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  if (strcmp(p, "OK")) return false; // fail

  // HTTP method action (0 = GET, 1 = POST)
  Serial.println(F("AT+HTTPACTION=1"));
  sim800l.println(F("AT+HTTPACTION=1"));
  delay(6000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  //if (strcmp(p, "OK")) return false; // fail


  // Terminate HTTP service
  Serial.println(F("AT+HTTPTERM"));
  sim800l.println(F("AT+HTTPTERM"));
  delay(3000);
  fgets_sim800l(buff, 32);
  Serial.println(buff);
  p = buff+strlen(buff)-2;
  //if (strcmp(p, "OK")) return false; // fail

  Serial.println(F("AT+CIPSHUT"));
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

  if (!SD.begin(10)) {
    Serial.println(F("SD card initialization failed!"));
    while (1);
  }

  File conf_file = SD.open(F("conf.txt"), FILE_READ);
  if (!conf_file) {
    Serial.println(F("Failed to open conf.txt file"));
    while (1);
  }

  char field[10];
  char value[25];
  int len = conf_file.available();
  uint8_t i;
  while (len) {
    i = -1;
    
    // get field
    do {
      i++;
      field[i] = conf_file.read();
      len--;
    } while (field[i] != ':' && len > 0);
    field[i] = '\0';

    // consume whitespaces
    while (conf_file.peek() == ' ') {
      conf_file.read();
      len--;
    }
    i = -1;

    // get value
    do {
      i++;
      value[i] = conf_file.read();
      len--;
    } while (value[i] != '\n' && len > 0);
    value[i] = '\0';

    if (strcmp(field, "bus_line") == 0) {
      strcpy(bus_line, value);
    } else if (strcmp(field, "apn") == 0) {
      strcpy(apn, value);
    } else if (strcmp(field, "url") == 0) {
      strcpy(url, value);
    } else if (strcmp(field, "port") == 0) {
      strcpy(port, value);
    } else {
      Serial.println(field);
    }
  }

  conf_file.close();

  if (!strcmp(bus_line,"") && !strcmp(apn,"") && !strcmp(url,"") && !strcmp(port,"")) {
    Serial.println(F("Missing values in conf.txt file."));
    while (1);
  }

  Serial.println(F("Setup done."));
}


void loop() {
  SerialGPS.listen();

  while (SerialGPS.available()) {
    if (GPS.encode(SerialGPS.read())) {
      float lat, lon;
      char lat_str[12], lon_str[12];
      unsigned long date, hour;
      char d_str[12], h_str[12];
      char speed_str[8];
      //unsigned short sat;
      char ts[20];

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
      sprintf(data, "{\"bus_line\": \"%s\",\"ts\": \"%s\",\"lat\": %s,\"lon\": %s,\"speed\": %s}", bus_line, ts, lat_str, lon_str, speed_str);
      Serial.println(data);
      
      // writing to file
      sprintf(filename, "%s%c%c.txt", d_str, h_str[0], h_str[1]);

      gps_data_file = SD.open(filename, FILE_WRITE);
      if (gps_data_file) {
        Serial.print(F("Writing data to file..."));
        gps_data_file.println(data);
        gps_data_file.close();
        Serial.println(F("Done."));
        data_count++;
      } else {
        Serial.println(F("Error opening data file"));
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
          Serial.println(F("Fail to send via GPRS"));
        }
        
        data_count = 0; // reset
      }
      delay(9000); // aprox 10 sec for each gps reading
    }
  }
}
