#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <Time.h>

int windSpdPin[3] = { A0, A1, A2 };  // A0, A1, A2: Wind speed sensor output
int windDirPin[3] = { A3, A6, A7 };  // A3, A6, A7: Wind direction sensor output
// A2-A3, A6-A7: Other sensors

static const int GPSRXPin = 2, GPSTXPin = 3;
static const uint32_t GPSBaud = 9600;
TinyGPSPlus gps;
SoftwareSerial gpsSerial(GPSRXPin, GPSTXPin);

// D0 - D5: UART
int DHT_PIN[3] = { 2, 3, 4 };  // D4, D5, D6: DHT22 sensor output
#define DHT_TYPE DHT22
// D10 - D13: SPI - Ethernet

int windSpdSensorVal[3] = { 0, 0, 0 };
int windDirSensorVal[3] = { 0, 0, 0 };

float windSpeed[3] = { 0, 0, 0 };
char windDirection[3][3] = { "N ", "N ", "N " };
float humidity[3] = { 0, 0, 0 };
float temperature[3] = { 0, 0, 0 };

unsigned long nextSendTime = 0, readSensorTime = 0;

LiquidCrystal_I2C lcd(0x27, 20, 4);
// DHT dht1(DHT_PIN[0], DHT_TYPE), dht2(DHT_PIN[1], DHT_TYPE), dht3(DHT_PIN[2], DHT_TYPE);
DHT dht[] = {
  { DHT_PIN[0], DHT_TYPE },
  { DHT_PIN[1], DHT_TYPE },
  { DHT_PIN[2], DHT_TYPE },
};

struct SlaveConfig {
  bool set1Enabled;
  bool set2Enabled;
  bool set3Enabled;
  long sendInterval;

  String toString() {
    return "set1Enabled=" + String(set1Enabled) + ", set2Enabled=" + String(set2Enabled) + ", set3Enabled=" + String(set3Enabled);
  }
};

SlaveConfig slaveConfig;

void readSlaveConfig() {
  EEPROM.get(0, slaveConfig);
  loadDefaultConfig();
}

void loadDefaultConfig() {
  // slaveConfig.set1Enabled = true;
  // slaveConfig.set2Enabled = true;
  // slaveConfig.set3Enabled = true;
  if (slaveConfig.sendInterval < 60000)
    slaveConfig.sendInterval = 60000;
  saveSlaveConfig();
}

void saveSlaveConfig() {
  EEPROM.put(0, slaveConfig);
}

void setWindDirection(int set, int value) {
  if (value < 100) {
    memcpy(windDirection[set], "N ", 2);
  } else if (value < 250) {
    memcpy(windDirection[set], "NE", 2);
  } else if (value < 384) {
    memcpy(windDirection[set], "E ", 2);
  } else if (value < 512) {
    memcpy(windDirection[set], "SE", 2);
  } else if (value < 640) {
    memcpy(windDirection[set], "S ", 2);
  } else if (value < 768) {
    memcpy(windDirection[set], "SW", 2);
  } else if (value < 930) {
    memcpy(windDirection[set], "W ", 2);
  } else {
    memcpy(windDirection[set], "NW", 2);
  }
}

void printGps() {
  if (gps.location.isValid()) {
    Serial.print(F("AT+LOG=Latitude: "));
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(" Longitude: "));
    Serial.println(gps.location.lng(), 6);
  } else {
    Serial.println(F("AT+LOG=Location not valid"));
  }
}

struct tm t = { 0 };

void sendData() {
  printGps();
  t.tm_year = gps.date.year() - 1870;
  t.tm_mon = gps.date.month() - 1;
  t.tm_mday = gps.date.day();
  t.tm_hour = gps.time.hour();
  t.tm_min = gps.time.minute();
  t.tm_sec = gps.time.second();
  time_t timeSinceEpoch = mktime(&t) - 86400;
  // Serial.println(timeSinceEpoch);
  // Example:
  //   {
  //     "timestamp": 1694546607,
  //     "location": {
  //         "lat": 52.202041606614195,
  //         "lon": 21.034210583578016
  //     },
  //     "records": [
  //         {
  //             "windSpeed": 1.0,
  //             "windDirection": "NW",
  //             "temperature": 27,
  //             "humidity": 61.6
  //         }
  //     ]
  // }
  String data = "{\"ts\":" + String(timeSinceEpoch) + ",\"loc\":{\"lat\":" + String(gps.location.lat(), 6) + ",\"lon\":" + String(gps.location.lng(), 6) + "},\"rcs\":[";
  for (int i = 0; i < 1; i++) {
    data += "{\"ws\":" + String(windSpeed[i]) + ",\"wd\":\"" + String(windDirection[i]) + "\",\"t\":" + String(temperature[i]) + ",\"h\":" + String(humidity[i]) + "}";
    if (i < 1 - 1)
      data += ",";
  }
  data += "]}";
  Serial.println("AT+DATA=" + data);
}

void setup() {
  // put your setup code here, to run once:
  readSlaveConfig();
  Wire.begin();
  for (auto &sensor : dht) {
    sensor.begin();
  }
  Serial.begin(9600);
  gpsSerial.begin(GPSBaud);

  // Initialize LCD
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Welcome to");
  lcd.setCursor(0, 1);
  lcd.print("Weather Station");
  lcd.setCursor(0, 2);
  lcd.print("Monitor");

  nextSendTime = millis() + slaveConfig.sendInterval;
}

void loop() {
  if (readSensorTime < millis()) {
    readSensorTime = millis() + 1000;
    lcd.clear();
    for (int i = 0; i < 1; i++) {
      windSpdSensorVal[i] = analogRead(windSpdPin[i]);
      windSpeed[i] = map(windSpdSensorVal[i], 0, 1023, 0, 300);
      windSpeed[i] = windSpeed[i] / 10;
      lcd.setCursor(0, 0);
      lcd.print("Speed: ");
      lcd.print(windSpeed[i]);

      windDirSensorVal[i] = analogRead(windDirPin[i]);
      setWindDirection(i, windDirSensorVal[i]);
      lcd.setCursor(0, 1);
      lcd.print("Direction: ");
      lcd.print(windDirection[i]);

      humidity[i] = dht[i].readHumidity();
      temperature[i] = dht[i].readTemperature();
      if (isnan(humidity[i]) || isnan(temperature[i])) {
        Serial.println(F("AT+LOG=Failed to read from DHT sensor!"));
      } else {
        lcd.setCursor(0, 2);
        lcd.print("Humidity: ");
        lcd.print(humidity[i]);
        lcd.print("%");
        lcd.setCursor(0, 3);
        lcd.print("Temperature: ");
        lcd.print(temperature[i]);
        lcd.print(char(223));
        lcd.print("C");
      }
    }
  }

  // Handle GPS data
  if (gpsSerial.available() > 0) {
    while (gpsSerial.available() > 0)
      gps.encode(gpsSerial.read());
  }

  if (millis() >= nextSendTime) {
    nextSendTime = millis() + slaveConfig.sendInterval;
    sendData();
  }
}