#include <SD.h>

// external libraries
#include <TinyGPS.h>

#define SEND_AFTER 5 // send data after 10 reads
#define DATA_SZ 120
#define sim800l Serial1
#define SerialGPS Serial2

// CMD to allow monitor serial in linux
// sudo chmod a+rw /dev/ttyACM0

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

char bus_id[12] = "";
uint8_t trip_id = 1;
char data[DATA_SZ+1]; // data to be stored


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


void rm_f_zeros(char *s) {
  uint8_t i = strlen(s)-1;

  while (i > 2 && s[i] == '0') {
    s[i] = '\0';
    i--;
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


// send file "filename" via HTTP POST
bool send_from_file(File data_file) {
  if (!data_file.available()) return false;

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


  //gps_data_file = SD.open(filename, FILE_READ);
  //if (!gps_data_file || !gps_data_file.available()) return false;


  // Input HTTP data (Sending data size and time limit(ms))
  sim800l.print(F("AT+HTTPDATA="));
  sim800l.print(data_file.available()); // number of bytes to read from file
  send_at_cmd(F(",100000"), response, 6000);
  if (strstr(response, "DOWNLOAD") == NULL) return false; // fail

  // Sending data
  while (data_file.available()) {
    uint8_t i = 0;
    memset(data, '\0', DATA_SZ); // reset string
    char c;
    do {
      data[i] = data_file.read();
      i++;
      c = data_file.peek();
    } while (c != '\n' && c != '\r');


    do {
      data_file.read();
      c = data_file.peek();
    } while (c == '\n' || c == '\r');

    Serial.print(F("Sending-> "));
    Serial.print(data);
    sim800l.println(data);
    Serial.print(F(" | Remaining Bytes: "));
    Serial.println(data_file.available());
  }
  sim800l_read(response, 10000);
  if (strstr(response, "OK") == NULL) return false; // fail
  //gps_data_file.close();

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

  if (!SD.begin(53)) {
    Serial.println(F("SD card initialization failed!"));
    while (1);
  }

  if (!SD.exists(F("conf.txt"))) {    
    Serial.println(F("conf.txt doesn't exists!"));
    while (1);
  }

  File conf_file = SD.open(F("conf.txt"), FILE_READ);
  if (!conf_file) {
    Serial.println(F("Failed to open conf.txt file!"));
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

    if (strcmp(field, "bus_id") == 0) {
      strcpy(bus_id, value);
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

  if (!strcmp(bus_id,"") || !strcmp(apn,"") || !strcmp(url,"") || !strcmp(port,"")) {
    Serial.println(F("Missing values in conf.txt file."));
    while (1);
  }

  Serial.println(F("Setup done."));
}


void loop() {
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

      rm_f_zeros(lat_str);
      rm_f_zeros(lon_str);
      rm_f_zeros(speed_str);
      
      // buid data
      sprintf(data,
        "{\"bus_id\":\"%s\",\"trip_id\":\"%s;%d\",\"ts\":\"%s\",\"lat\":%s,\"lon\":%s,\"speed\":%s}",
        bus_id, bus_id, trip_id,ts, lat_str, lon_str, speed_str
      );
      Serial.println(data);
      
      // writing to file
      //sprintf(filename, "%s%c%c.txt", d_str, h_str[0], h_str[1]);
      sprintf(filename, "%s.txt", "data");

      uint8_t file_exists = SD.exists(filename);

      gps_data_file = SD.open(filename, O_WRITE | O_CREAT);
      if (gps_data_file) {
        Serial.print(F("Writing data to file..."));
        if (file_exists) {
          unsigned long f_size = gps_data_file.size();
          if (!gps_data_file.seek(f_size-2)) {
            gps_data_file.close();
            break;
          };

          gps_data_file.println(F(","));
          gps_data_file.print(data);
        } else {
          gps_data_file.println(F("{\"data\":"));
          gps_data_file.print(F("["));
          gps_data_file.print(data);
        }
        gps_data_file.print(F("]}"));
        
        gps_data_file.close();
        Serial.println(F("Done."));
        data_count++;
      } else {
        Serial.println(F("Error opening data file"));
      }

      if (data_count == SEND_AFTER) {
        if (sim800l_ready()) {
          gps_data_file = SD.open(filename, FILE_READ);
          if (gps_data_file) {
            bool success = send_from_file(gps_data_file);
            gps_data_file.close();
            if (success) SD.remove(filename);
          }

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
