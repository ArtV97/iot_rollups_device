#include <SD.h>
#include <Crypto.h>
#include <SHA256.h>     // cryptographic hash function
#include <Ed25519.h>    // asymetric cryptograph


#define SD_CS_PIN SS


#define HASH_SIZE 32
#define DATA_SZ 100

// Ed25519
const uint8_t privateKey[32] = {
                    0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60,
                    0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4,
                    0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19,
                    0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60
                    };

const uint8_t publicKey[32] = {
                    0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7,
                    0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
                    0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25,
                    0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a
                    };



void build_sha256(byte hash_value[HASH_SIZE]) {
    char data[DATA_SZ+1]; // data to be stored
    SHA256 sha256;
    sha256.reset(); // clear previous hash information
    File gps_data_file = SD.open("04102215.txt", FILE_READ);
    int line_counter = 0;
    int total_bytes = 0;
    if (gps_data_file) {
        while (gps_data_file.available()) {
            uint8_t i = 0;

            char c;
            do {
                data[i] = gps_data_file.read();
                i++;
                c = gps_data_file.peek();
            } while (gps_data_file.available() && c != '\n' && c != '\r');
            data[i] = '\0';
            line_counter++;

            Serial.println(data);
            sha256.update(data, strlen(data));

            do {
                gps_data_file.read();
                c = gps_data_file.peek();
            } while (gps_data_file.available() && (c == '\n' || c == '\r'));
        }
    gps_data_file.close();

    sha256.finalize(hash_value, HASH_SIZE);
    // Serial.print("Hash: ");
    // for (int i = 0; i < HASH_SIZE; i++) {
    //     if (hash_value[i] < 16) {
    //         Serial.print(F("0"));
    //     }
    //     Serial.print(hash_value[i], HEX);
    // }
    // Serial.println();

    // sign_message(hash_value);
    } else {
        Serial.println(F("Couldn't open file."));
    }
}


void sign_message(byte *msg) {
    Serial.print("Hash2: ");
    for (int i = 0; i < HASH_SIZE; i++) {
        if (msg[i] < 16) {
            Serial.print(F("0"));
        }
        Serial.print(msg[i], HEX);
    }
    Serial.println("\n\n");
    delay(200);
    byte signature[64];
    Ed25519::sign(signature, privateKey, publicKey, msg, HASH_SIZE);
    Serial.print("Signed Hash: ");
    for (int i = 0; i < 64; i++) {
        if (signature[i] < 16) {
            Serial.print(F("0"));
        }
        Serial.print(signature[i], HEX);
    }
    Serial.println();

    // if (!Ed25519::verify(signature, publicKey, msg, HASH_SIZE)) {
    //     Serial.println("Invalid Signature!");
    // } else {
    //     Serial.println("Valid Signature!");
    // }
}


void setup() {
    Serial.begin(9600);


    if (!SD.begin(SD_CS_PIN)) {
        Serial.println(F("SD card initialization failed!"));
        while (1);
    }

    if (Serial.availableForWrite()) {
        Serial.flush();
    }
    
    Serial.println("\n--- BEGIN ---");
    //memset(data, '\0', DATA_SZ); // reset string
    
    byte hash_value[HASH_SIZE];
    build_sha256(hash_value);
    Serial.print("Hash: ");
    for (int i = 0; i < HASH_SIZE; i++) {
        if (hash_value[i] < 16) {
            Serial.print(F("0"));
        }
        Serial.print(hash_value[i], HEX);
    }
    Serial.println("\n\n");
    delay(200);

    sign_message(hash_value);

    Serial.print("Private Key: ");
    for (byte i = 0; i < 32; i++) {
        if (privateKey[i] < 16) {
            Serial.print(F("0"));
        }
        Serial.print(privateKey[i], HEX);
    }
    Serial.println();

    Serial.print("Public Key: ");
    for (byte i = 0; i < 32; i++) {
        if (publicKey[i] < 16) {
            Serial.print(F("0"));
        }
        Serial.print(publicKey[i], HEX);
    }


    Serial.println("\n\n");
    Serial.println("--- END  ---");
}

void loop() {

}
