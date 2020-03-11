#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LoRa.h>
#include <SPIFFS.h>
#include <Time.h>
#include <TimeLib.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <Wire.h>

// Board Details
#define NAME "TTGO T-Beam v0.7"

#define LORA_IO1 33
#define LORA_IO2 32
#define LORA_RESET 23
#define LORA_SEL 18
#define LORA_SCK 5
#define LORA_MOSI 27
#define LORA_MISO 19
#define LORA_IO0 26

#define GPS_TX 12
#define GPS_RX 15

#define USER_BUTTON 39

#define KEEPALIVE_MS 60000

TinyGPSPlus gps;
HardwareSerial GPSSerial(1);
AsyncWebServer server(80);

#define CSVPATH_FORMAT "/recv_%04i.csv"
#define CSVPATH_LEN 50
#define CSV_HEADER                                                             \
    "ts, gps_sat, gps_age, lat, lon, alt, "                                    \
    "cnt, len, rssi, snr, freq_err, msg"

// wifi configuration
const char *sta_ssid = "";
const char *sta_password = "";

const char *ap_ssid = "TTGO T-Beam LoRa Logger";
const char *ap_password = "supersicher";

// lora config
int lora_coding_demoninator = 5;
int lora_signal_bandwidth = 125E3;
int lora_spreading_factor = 7;
int lora_frequency = 868E6;

// globals
char csvpath[CSVPATH_LEN];
File csvfile;

File openLog() {
    int run = 0;
    do {
        run++;
        snprintf(csvpath, CSVPATH_LEN, CSVPATH_FORMAT, run);
    } while (SPIFFS.exists(csvpath));

    csvfile = SPIFFS.open(csvpath, FILE_WRITE);
    return csvfile;
}

#define TS_LEN 32
char _ts[TS_LEN];

char *ts() {
    snprintf(_ts, TS_LEN, "%4i-%02i-%02i %02i:%02i:%02i", year(), month(),
             day(), hour(), minute(), second());
    return _ts;
}

void write_csv(int cnt, int len, int rssi, float snr, long freq_err,
               char *buf) {
    csvfile.printf("%s, %i, %i, %f, %f, %f, %i, %i, %i, %f, %li, \"%s\"\n",
                   ts(), gps.satellites.value(), gps.satellites.age(),
                   gps.location.lat(), gps.location.lng(),
                   gps.altitude.meters(), cnt, len, rssi, snr, freq_err, buf);
    csvfile.flush();
}

void receive_callback(int lora_len) {
    static int lora_cnt = 0;
    char lora_head[4];
    char lora_buf[256];

    LoRa.readBytes(lora_head, 4);
    lora_len = LoRa.readBytes(lora_buf, lora_len - 4);
    lora_buf[lora_len] = '\0';
    lora_cnt++;

    Serial.printf("%s: Received LoRa message #%i (%i bytes): '%s'\n", ts(),
                  lora_cnt, lora_len, lora_buf);

    write_csv(lora_cnt, lora_len, LoRa.packetRssi(), LoRa.packetSnr(),
              LoRa.packetFrequencyError(), lora_buf);
}

void setup() {
    Serial.begin(115200);
    Serial.println(NAME);

    // Initialize SPIFFS + open Logfile
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS: Error mounting filesystem");
    }

    pinMode(USER_BUTTON, INPUT);
    if (!digitalRead(USER_BUTTON)) {
        Serial.printf("Keep button pressed for 3 seconds to format storage.");
        for (int i = 0; i < 30; i++) {
            delay(100);
            if (digitalRead(USER_BUTTON)) {
                Serial.printf(" abort.\n");
                break;
            }
            Serial.printf(".");
        }

        // remove files
        if (!digitalRead(USER_BUTTON)) {
            Serial.printf(" formatting storage...");
            SPIFFS.format();
            Serial.printf(" done.\n");
        }
    }

    if (!openLog()) {
        Serial.println("Log: Error opening csv file");
    } else {
        Serial.printf("Opened csv file %s\n", csvpath);
    }

    // One Wire Bus
    if (!Wire.begin(21, 22)) {
        Serial.println("One Wire: begin error");
    }

    // GPS
    GPSSerial.begin(9600, SERIAL_8N1, GPS_TX, GPS_RX);

    // Init LoRa Radio
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SEL);

    LoRa.setPins(LORA_SEL, LORA_RESET, LORA_IO0);
    if (!LoRa.begin(868E6)) {
        Serial.println("LoRa Radio: init failed.");
    }
    LoRa.setCodingRate4(lora_coding_demoninator);
    LoRa.setSignalBandwidth(lora_signal_bandwidth);
    LoRa.setSpreadingFactor(lora_spreading_factor);
    LoRa.setFrequency(868E6);

    // WiFi
    if (WiFi.begin(sta_ssid, sta_password)) {
        unsigned long timeout = millis() + 5000;
        while (millis() < timeout) {
            if (WiFi.status() == WL_CONNECTED)
                break;
            delay(100);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi: Connected to %s, IP: %s\n", sta_ssid,
                      WiFi.localIP().toString().c_str());

    } else if (!WiFi.softAP(ap_ssid, ap_password))
        Serial.printf("WiFi: AP SSID: %s, IP: %s\n", ap_ssid,
                      WiFi.localIP().toString().c_str());
    else
        Serial.printf("WiFi: AP and STA mode failed.");

    // WebServer
    server.serveStatic("/", SPIFFS, "/");
    server.addRewrite(new AsyncWebRewrite("/", csvpath));
    server.begin();

    // Print CSV header
    csvfile.printf(CSV_HEADER "\n");
    csvfile.flush();
}

void loop() {

    // check if a new packet is available
    int lora_avail = LoRa.parsePacket();
    if (lora_avail)
        receive_callback(lora_avail);

    // consume GPS information
    while (GPSSerial.available()) {
        const char gps_data = GPSSerial.read();
        if (gps.encode(gps_data)) {

            if (gps.time.isUpdated()) {
                setTime(gps.time.hour(), gps.time.minute(), gps.time.second(),
                        gps.date.day(), gps.date.month(), gps.date.year());

                Serial.printf("%s: Time set according to GPS\n", ts());
            }

            if (gps.location.isUpdated()) {
                Serial.printf("%s: GPS position: %f, %f, alt: %f\n", ts(),
                              gps.location.lat(), gps.location.lng(),
                              gps.altitude.meters());
            }
        }
    }

    static unsigned long next_keepalive = millis();

    if (millis() >= next_keepalive) {
        Serial.printf("%s: Printing keepalive\n", ts());
        char empty[] = "";
        write_csv(0, 0, 0, 0.0, 0, empty);
        next_keepalive += KEEPALIVE_MS;
    }

    delay(10);
}