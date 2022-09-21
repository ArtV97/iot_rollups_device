#include<SoftwareSerial.h>
#include<TinyGPS.h>
#include <SD.h>

#define SEND_AFTER 2 // send data after 10 reads

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

void sim800l_clear() {
  while (sim800l.available()) { sim800l.read(); }
}

void sim800l_read(char *buffer, int wait_ms) {
  uint8_t i = 0;
  unsigned long begin;
  
  begin = millis();

  do {
    if (sim800l.available()) {
      buffer[i] = sim800l.read();
      i++;
    }
  } while (millis() - begin <= wait_ms);

  buffer[i] = '\0';
  Serial.println(buffer);
}


template <typename type>
void send_at_cmd(type cmd, char *response, int wait_ms) {
  sim800l_clear();
  sim800l.println(cmd);
  sim800l_read(response, wait_ms);
}


bool sim800l_ready() {
  sim800l.listen();
  char response[100]; // AT command response
  
  // AT commands reconized
  send_at_cmd(F("AT"), response, 1500);
  if (strstr(response, "OK") == NULL) return false; // fail

  // is sim card ok
  send_at_cmd(F("AT+CPIN?"), response, 1500);
  if (strstr(response, "READY") == NULL) return false; // fail

  // Signal Quality
  send_at_cmd(F("AT+CSQ"), response, 2000);
  if (strstr(response, "OK") == NULL) return false; // fail

  // Display the current network operator currently registered.
  send_at_cmd(F("AT+COPS?"), response, 3000);
  if (strstr(response, "+COPS: 0\n") != NULL) return false; // fail (no operator)
  if (strstr(response, "OK") == NULL) return false; // fail

  return true;
}


bool send_data(char *data) {
  char response[100]; // AT command response

  // set bearer parameters (Type of Internet connection, "GPRS" or "CSD")
  send_at_cmd(F("AT+SAPBR=3,1,\"Contype\",\"GPRS\""), response, 5000);
  if (strstr(response, "OK") == NULL) return false; // fail

  // set bearer parameters (access point name)
  sim800l.print(F("AT+SAPBR=3,1,\"APN\",\""));
  sim800l.print(apn);
  send_at_cmd(F("\""), response, 6000);
  if (strstr(response, "OK") == NULL) return false; // fail

  // open bearer
  send_at_cmd(F("AT+SAPBR=1,1"), response, 15000);
  if (strstr(response, "OK") == NULL) {
    // close bearer
    send_at_cmd(F("AT+SAPBR=0,1"), response, 10000);

    // try again
    send_at_cmd(F("AT+SAPBR=1,1"), response, 15000);
    if (strstr(response, "OK") == NULL) return false; // fail
  }

  // query bearer
  send_at_cmd(F("AT+SAPBR=2,1"), response, 6000);
  if (strstr(response, "OK") == NULL) return false; // fail

  // Initialize HTTP service
  send_at_cmd(F("AT+HTTPINIT"), response, 5000);
  //if (strstr(response, "OK") == NULL) return false; // fail
  if (strstr(response, "OK") == NULL) {
    // terminate
    send_at_cmd(F("AT+HTTPTERM"), response, 3000);

    // try again
    send_at_cmd(F("AT+HTTPINIT"), response, 5000);
    if (strstr(response, "OK") == NULL) return false; // fail
  }

  // Set HTTP parameters value (Bearer profile identifier) * Mandatory parameter
  send_at_cmd(F("AT+HTTPPARA=\"CID\",1"), response, 6000);
  if (strstr(response, "OK") == NULL) return false; // fail

  // Server address
  sim800l.print(F("AT+HTTPPARA=\"URL\",\"http://"));
  sim800l.print(url);
  sim800l.print(F(":"));
  sim800l.print(port);
  send_at_cmd(F("\""), response, 6000);
  if (strstr(response, "OK") == NULL) return false; // fail

  // Set the “Content-Type” field in HTTP header
  send_at_cmd(F("AT+HTTPPARA=\"CONTENT\",\"application/json\""), response, 4000);
  if (strstr(response, "OK") == NULL) return false; // fail

  // Input HTTP data (Sending data size and time limit(ms))
  sim800l.print(F("AT+HTTPDATA="));
  sim800l.print(strlen(data));
  send_at_cmd(F(",100000"), response, 6000);
  if (strstr(response, "DOWNLOAD") == NULL) return false; // fail

  // Sending data
  sim800l.print(data);
  send_at_cmd(F(""), response, 6000);
  if (strstr(response, "OK") == NULL) return false; // fail

  // HTTP method action (0 = GET, 1 = POST)
  bool post_success = true;
  send_at_cmd(F("AT+HTTPACTION=1"), response, 6000);
  if (strstr(response, "OK") == NULL) return false; // fail
  if (strstr(response, "1,200") == NULL) post_success = false;

  // Terminate HTTP service
  send_at_cmd(F("AT+HTTPTERM"), response, 3000);
  //if (strstr(response, "OK") == NULL) return false; // fail

  send_at_cmd(F("AT+CIPSHUT"), response, 2000);
  //if (strstr(response, "OK") == NULL) return false; // fail

  return post_success;
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
      } else {
        delay(9000); // aprox 10 sec for each gps reading
      }
    }
  }
}
