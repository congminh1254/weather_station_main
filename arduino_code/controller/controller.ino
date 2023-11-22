#define TINY_GSM_MODEM_SIM800
// #if !defined(TINY_GSM_RX_BUFFER)
// #define TINY_GSM_RX_BUFFER 300
// #endif

#include <SPI.h>
#include <Ethernet.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <Time.h>

enum ATMessageSource
{
  AT_Serial,
  AT_BLE,
  AT_SIM,
  AT_WIFI,
  AT_LORA,
  AT_ETH
};

#define maxWindSensor 16
#define maxDhtSensor 8

#define gpsSerial Serial1
#define bleSerial Serial2
#define loraSerial Serial3
static const uint32_t GPSBaud = 9600;
TinyGPSPlus gps;
SoftwareSerial wifiSerial(12, 13); // RX, TX
SoftwareSerial simSerial(10, 11);  // RX, TX

// D0 - D5: UART
#define DHT_TYPE DHT22

char serialAnswer[700];
ATMessageSource messageSource;
char dataBuffer[500], loraBuffer[500], wifiBuffer[500], simBuffer[500];
int windSpdSensorVal[maxWindSensor]; // Wind speed sensor output
int windDirSensorVal[maxWindSensor]; // Wind direction sensor output
float windSpeed[maxWindSensor];
char windDirection[maxWindSensor][3];
float humidity[maxDhtSensor];
float temperature[maxDhtSensor];

unsigned long nextSendTime = 0, readSensorTime = 0;
unsigned long readTimeout = 0;
int readTried = 0;
int serialAvailable = 0;

// GPS Configuration
TinyGsm *sim800l;
TinyGsmClient *sim800lclient;

// SIM Configuration
const char APN[] = "internet";

// LORA Configuration
const int MAX_LORA_LENGTH = 36;
bool loraSending = false;
int loraCurrentPos = 0;
unsigned long loraSendTimeout = 0;

// Wifi Configuration
bool wifiSending = false;
int wifiCurrentStep = 0;
unsigned long wifiSendTimeout = 0;

byte ethMac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

LiquidCrystal_I2C lcd(0x27, 20, 4);
// DHT dht1(DHT_PIN[0], DHT_TYPE), dht2(DHT_PIN[1], DHT_TYPE), dht3(DHT_PIN[2], DHT_TYPE);
DHT dht[maxDhtSensor] = {
    {2, DHT_TYPE},
    {3, DHT_TYPE},
    {4, DHT_TYPE},
    {5, DHT_TYPE},
    {6, DHT_TYPE},
    {7, DHT_TYPE},
    {8, DHT_TYPE},
    {9, DHT_TYPE}};

const char serverProtocol[] = "http";
const char serverSecuredProtocol[] = "https";
const char serverHost[] = "weather-api.ncminh.dev";
const char serverAddWeatherDataPath[] = "/weatherData";

struct SensorsConfig
{
  bool initialized;
  bool DHT_ENABLED[maxDhtSensor]; // DHT22 sensor enabled (From D2 to D9)
  int windDirPin[maxWindSensor];  // Wind direction sensor output (Analogue)
  int windSpdPin[maxWindSensor];  // Wind speed sensor output (Analogue)

  String toString()
  {
    return "SensorsConfig";
  }
};

struct WifiConfig
{
  char ssid[16];
  char pwd[16];
};
struct SimConfig
{
  char apn[16];
  char user[16];
  char pwd[16];
};
struct LoraConfig
{
  char devEui[16];
  char appEui[16];
  char appKey[32];
};
struct LocationInfo
{
  double lat;
  double lon;

  String toString()
  {
    return "LocationInfo=" + String(lat) + "," + String(lon);
  }
};

struct CommunicationConfig
{
  bool initialized;
  bool bleEnabled;
  bool simEnabled;
  SimConfig simConfig;
  bool wifiEnabled;
  WifiConfig wifiConfig;
  bool loraEnabled;
  LoraConfig loraConfig;
  bool ethEnabled;
  int stationId;
  long sendInterval;

  LocationInfo lastLocation;

  String toString()
  {
    return "Station " + String(stationId) + " " + String(bleEnabled) + String(simEnabled) + String(wifiEnabled) + String(loraEnabled) + String(ethEnabled);
  }
};

struct DeviceState
{
  bool loraInitialized;
  bool bleInitialized;
  bool simInitialized;
  bool wifiInitialized;
  bool ethInitialized;
  bool gpsInitialized;
};

SensorsConfig sensorConfig;
CommunicationConfig comConfig;
DeviceState deviceState;

void resetData()
{
  memset(&windSpeed, -1, sizeof(windSpeed));
  memset(&humidity, -1, sizeof(humidity));
  memset(&temperature, -1, sizeof(temperature));
  memset(&windDirection, 0, sizeof(windDirection));
  memset(&deviceState, 0, sizeof(deviceState));
}

void readConfig()
{
  EEPROM.get(0, sensorConfig);
  if (!sensorConfig.initialized)
  {
    loadDefaultSensorConfig();
    saveSensorConfig();
  }

  EEPROM.get(sizeof(sensorConfig), comConfig);
  if (!comConfig.initialized)
  {
    loadDefaultCommunicationConfig();
    saveComConfig();
  }
}

void loadDefaultSensorConfig()
{
  sensorConfig.initialized = true;
  memset(&sensorConfig.DHT_ENABLED, false, sizeof(sensorConfig.DHT_ENABLED));
  memset(&sensorConfig.windDirPin, -1, sizeof(sensorConfig.windDirPin));
  memset(&sensorConfig.windSpdPin, -1, sizeof(sensorConfig.windSpdPin));
  saveSensorConfig();
}

void loadDefaultCommunicationConfig()
{
  comConfig.initialized = true;
  strcpy(comConfig.wifiConfig.ssid, "CGA2121_aaXyMrx");
  strcpy(comConfig.wifiConfig.pwd, "dolna2a72");

  strcpy(comConfig.simConfig.apn, "internet");
  strcpy(comConfig.simConfig.user, "");
  strcpy(comConfig.simConfig.pwd, "");

  strcpy(comConfig.loraConfig.devEui, "6081F9F7B8151EE5");
  strcpy(comConfig.loraConfig.appEui, "6081F9E9F6B003F9");
  strcpy(comConfig.loraConfig.appKey, "55F5B1B2CE8DED19A3DB57FD976D9C05");

  comConfig.stationId = random(1000, 9999);
  comConfig.sendInterval = 60000;
  saveComConfig();
}

void saveSensorConfig()
{
  EEPROM.put(0, sensorConfig);
}

void saveComConfig()
{
  EEPROM.put(sizeof(sensorConfig), comConfig);
}

void setWindDirection(int set, int value)
{
  memset(windDirection[set], 0, sizeof(windDirection[set]));
  if (value < 100)
  {
    memcpy(windDirection[set], "N", 1);
  }
  else if (value < 250)
  {
    memcpy(windDirection[set], "NE", 2);
  }
  else if (value < 384)
  {
    memcpy(windDirection[set], "E", 1);
  }
  else if (value < 512)
  {
    memcpy(windDirection[set], "SE", 2);
  }
  else if (value < 640)
  {
    memcpy(windDirection[set], "S", 1);
  }
  else if (value < 768)
  {
    memcpy(windDirection[set], "SW", 2);
  }
  else if (value < 930)
  {
    memcpy(windDirection[set], "W", 1);
  }
  else
  {
    memcpy(windDirection[set], "NW", 2);
  }
}

void printGps()
{
  if (gps.location.isValid())
  {
    Serial.print(F("AT+LOG=Latitude: "));
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(" Longitude: "));
    Serial.println(gps.location.lng(), 6);
  }
  else
  {
    Serial.print(F("AT+LOG=Cached Latitude: "));
    Serial.print(comConfig.lastLocation.lat, 6);
    Serial.print(F(" Longitude: "));
    Serial.println(comConfig.lastLocation.lon, 6);
  }
}

LocationInfo getGps()
{
  LocationInfo location;
  if (gps.location.isValid())
  {
    location.lat = gps.location.lat();
    location.lon = gps.location.lng();
  }
  else
  {
    location.lat = comConfig.lastLocation.lat;
    location.lon = comConfig.lastLocation.lon;
  }
  return location;
}

// struct tm t = {0};

void sendData()
{
  printGps();

  // With timestamp
  // t.tm_year = gps.date.year() - 1870;
  // t.tm_mon = gps.date.month() - 1;
  // t.tm_mday = gps.date.day();
  // t.tm_hour = gps.time.hour();
  // t.tm_min = gps.time.minute();
  // t.tm_sec = gps.time.second();
  // time_t timeSinceEpoch = mktime(&t) - 86400;
  // String data = "{\"ts\":" + String(timeSinceEpoch) + ",\"loc\":{\"lat\":" + String(gps.location.lat(), 6) + ",\"lon\":" + String(gps.location.lng(), 6) + "},\"data\":{";

  // Without timestamp
  LocationInfo location = getGps();
  String data = "{\"loc\":{\"lat\":" + String(location.lat, 6) + ",\"lon\":" + String(location.lon, 6) + "},\"data\":{";

  data += "\"ws\":[";
  for (int i = 0; i < 8; i++)
  {
    if (sensorConfig.windSpdPin[i] != -1)
    {
      data += String(windSpeed[i]);
    }
  }
  // Remove last comma
  if (data[data.length() - 1] == ',')
  {
    data = data.substring(0, data.length() - 1);
  }
  data += "],\"wd\":[";

  for (int i = 0; i < 8; i++)
  {
    if (sensorConfig.windDirPin[i] != -1)
    {
      data += "\"" + String(windDirection[i]) + "\"";
    }
  }
  // Remove last comma
  if (data[data.length() - 1] == ',')
  {
    data = data.substring(0, data.length() - 1);
  }
  data += "],\"a\":["; // Air objects, temperature and humidity
  for (int i = 0; i < 8; i++)
  {
    if (sensorConfig.DHT_ENABLED[i])
    {
      data += "{\"t\":" + String(temperature[i]) + ",\"h\":" + String(humidity[i]) + "}";
    }
  }
  // Remove last comma
  if (data[data.length() - 1] == ',')
  {
    data = data.substring(0, data.length() - 1);
  }
  data += "]}}";
  Serial.println("AT+DATA=" + data);

  memcpy(dataBuffer, data.c_str(), data.length());
  if (comConfig.bleEnabled)
  {
    // Send data to BLE
  }
  if (comConfig.simEnabled)
  {
    // Send data to SIM
    memset(simBuffer, 0, sizeof(simBuffer));
    memcpy(simBuffer, data.c_str(), data.length());
    sendSimData();
  }
  if (comConfig.wifiEnabled)
  {
    // Send data to WIFI
    wifiCurrentStep = 0;
    wifiSending = true;
    memset(wifiBuffer, 0, sizeof(wifiBuffer));
    memcpy(wifiBuffer, data.c_str(), data.length());
    wifiSendTimeout = millis() + 120000;
    sendWifiData();
  }
  if (comConfig.loraEnabled)
  {
    // Send data to LORA
    loraCurrentPos = 0;
    loraSending = true;
    memset(loraBuffer, 0, sizeof(loraBuffer));
    memcpy(loraBuffer, data.c_str(), data.length());
    loraSendTimeout = millis() + 120000;
    sendLoraData();
  }
  if (comConfig.ethEnabled)
  {
    // Send data to ETH
  }
}

void resetLoraData()
{
  loraSending = false;
  loraCurrentPos = 0;
  memset(loraBuffer, 0, sizeof(loraBuffer));
}

void sendLoraData()
{
  // Lora package can send upto 51 bytes,
  // so we need to split the message into multiple packages
  // with format: x:y:deviceId:z with x is the current position of message,
  // y is the total length of message, z is the message content
  // length of each message is upto 36 bytes

  int totalLength = strlen(dataBuffer);
  int timeTried = 0;

  if (loraCurrentPos > 0)
  {
    memset(serialAnswer, 0, sizeof(serialAnswer));
    while (loraSerial.available())
      loraSerial.read();
    delay(200);
  }
  Serial.print(F("AT+LOG=Sending :"));
  loraSerial.print("AT+CMSG=\"");
  loraSerial.print(loraCurrentPos);
  loraSerial.print(":");
  loraSerial.print(totalLength);
  loraSerial.print(":");
  loraSerial.print(comConfig.stationId);
  loraSerial.print(":");

  for (int i = loraCurrentPos; i < loraCurrentPos + MAX_LORA_LENGTH; i++)
  {
    if (loraBuffer[i] == '"')
      loraBuffer[i] = '\'';
    if (i < totalLength)
    {
      loraSerial.print(loraBuffer[i]);
      Serial.print(loraBuffer[i]);
    }
    else
    {
      break;
    }
  }
  loraCurrentPos += MAX_LORA_LENGTH;

  loraSerial.println("\"");
  Serial.println("");

  if (loraCurrentPos >= totalLength)
  {
    resetLoraData();
  }
}

void resetWifiData()
{
  wifiSending = false;
  wifiCurrentStep = 0;
  memset(wifiBuffer, 0, sizeof(wifiBuffer));
}

void sendWifiData()
{
  switch (wifiCurrentStep)
  {
  case 0:
    Serial.println(F("AT+LOG=Sending WIFI data"));
    wifiSending = true;
    wifiSerial.print("AT+NEWREQ\n");
    break;
  case 1:
    wifiSerial.print("AT+URL=" + String(serverSecuredProtocol) + "://" + String(serverHost) + String(serverAddWeatherDataPath) + "?stationId=" + String(comConfig.stationId) + "&source=wifi\n");
    Serial.print("AT+URL=" + String(serverSecuredProtocol) + "://" + String(serverHost) + String(serverAddWeatherDataPath) + "?stationId=" + String(comConfig.stationId) + "&source=wifi\n");
    break;
  case 2:
    wifiSerial.print("AT+HEADER=content-type:application/json\n");
    Serial.print("AT+HEADER=content-type:application/json\n");
    break;
  case 3:
    wifiSerial.print("AT+POST=" + String(wifiBuffer) + "\n");
    Serial.print("AT+POST=" + String(wifiBuffer) + "\n");
    break;
  case 4:
    wifiSerial.print("AT+READ\n");
    break;
  case 5:
    wifiSerial.print("AT+ENDREQ\n");
    break;
  case 6:
    Serial.println(F("AT+LOG=WIFI data sent"));
    resetWifiData();
    break;
  }
  delay(50);
}

void resetSimData()
{
  memset(simBuffer, 0, sizeof(simBuffer));
}

void sendSimData()
{
  Serial.print(F("AT+LOG=Connecting to "));
  Serial.println(comConfig.simConfig.apn);
  if (!sim800l->gprsConnect(comConfig.simConfig.apn, comConfig.simConfig.user, comConfig.simConfig.pwd))
  {
    Serial.println(" fail");
    return;
  }
  Serial.println(" success");
  if (sim800l->isGprsConnected())
  {
    Serial.println("AT+LOG=GPRS connected");
  }
  Serial.print(F("AT+LOG=Local IP: "));
  Serial.println(sim800l->getLocalIP());
  Serial.print(F("AT+LOG=Connecting to "));
  Serial.println(serverHost);

  HttpClient http(*sim800lclient, serverHost, 80);
  http.setTimeout(60000);
  http.connectionKeepAlive();

  int err = http.post(serverAddWeatherDataPath + String("?stationId=") + String(comConfig.stationId) + String("&source=cellular"), "application/json", simBuffer);
  if (err != 0)
  {
    Serial.println(F("failed to connect"));
    sim800lclient->stop();
    delay(10000);
    return;
  }

  int status = http.responseStatusCode();
  Serial.print(F("AT+LOG=Response status code: "));
  Serial.println(status);
  if (status < 100)
  {
    Serial.println(F("AT+LOG=Request failed!"));
    sim800lclient->stop();
    http.stop();
    sim800l->gprsDisconnect();
    return;
  }
  if (!status)
  {
    delay(10000);
    return;
  }

  // Serial.println(F("Response Headers:"));
  // while (http.headerAvailable())
  // {
  //   String headerName = http.readHeaderName();
  //   String headerValue = http.readHeaderValue();
  //   Serial.println("    " + headerName + " : " + headerValue);
  // }

  int length = http.contentLength();
  if (length >= 0)
  {
    Serial.print(F("AT+LOG=Content length is: "));
    Serial.println(length);
  }
  if (http.isResponseChunked())
  {
    Serial.println(F("AT+LOG=The response is chunked"));
  }
  Serial.print(F("AT+LOG=Response:"));
  Serial.println(http.responseBody());

  // Shutdown
  http.stop();
  Serial.println(F("AT+LOG=Server disconnected"));
  sim800l->gprsDisconnect();
}

void processATCommand()
{
  if (messageSource == ATMessageSource::AT_Serial)
  {
    if (strstr(serialAnswer, "AT+DATA") != NULL)
    {
      // TODO: Implement data
    }
    else if (strstr(serialAnswer, "AT+CONFIG") != NULL)
    {
      // TODO: Implement config
    }
  }
  else if (messageSource == ATMessageSource::AT_LORA)
  {
    // Start with +CMSG: Log from sending LORA packet
    lowerCaseSerialAnswer();
    if (strstr(serialAnswer, "+cmsg:") != NULL)
    {
      if (strstr(serialAnswer, "done") != NULL)
      {
        Serial.println(F("AT+LOG=LORA data sent"));
        if (loraSending)
          sendLoraData();
      }
      else if (strstr(serialAnswer, "error") != NULL)
      {
        Serial.println(F("AT+LOG=Failed to send LORA data, cancelling"));
        resetLoraData();
      }
      else
      {
        // Another message, can be ignored
      }
    }
  }
  else if (messageSource == ATMessageSource::AT_WIFI)
  {
    // Start with +CMSG: Log from sending WIFI packet
    if (wifiSending)
    {
      wifiCurrentStep++;
      sendWifiData();
    }
  }
}

void trimSerialAnswer()
{
  // Trim trailing newline
  if (serialAnswer[strlen(serialAnswer) - 1] == '\n')
  {
    serialAnswer[strlen(serialAnswer) - 1] = '\0';
  }
  // Trim trailing carriage return
  if (serialAnswer[strlen(serialAnswer) - 1] == '\r')
  {
    serialAnswer[strlen(serialAnswer) - 1] = '\0';
  }
}

void lowerCaseSerialAnswer()
{
  for (int i = 0; i < strlen(serialAnswer); i++)
  {
    serialAnswer[i] = tolower(serialAnswer[i]);
  }
}

void readSerial(Stream *serial, int timeout)
{
  readTimeout = millis() + timeout;
  memset(serialAnswer, 0, sizeof(serialAnswer));

  // Read until newline or timeout
  while (millis() < readTimeout)
  {
    serialAvailable = serial->available();
    char ch = '\0';
    while (--serialAvailable >= 0)
    {
      ch = serial->read();
      if (ch == '\n')
      {
        break;
      }
      else
      {
        serialAnswer[strlen(serialAnswer)] = ch;
      }
      delay(1);
    }
    if (ch == '\n')
      break;
    delay(5);
  }
  if (millis() >= readTimeout)
  {
    Serial.println(F("AT+LOG=Timeout while reading from serial"));
  }
  serial->flush();
  trimSerialAnswer();
}

void setupSimModule()
{
  sim800l = new TinyGsm(simSerial);
  sim800lclient = new TinyGsmClient(*sim800l);

  sim800l->init();
  Serial.print(F("AT+LOG=Modem Info: "));
  Serial.println(sim800l->getModemInfo());
  Serial.print(F("AT+LOG=Modem Name: "));
  Serial.println(sim800l->getModemName());
  Serial.print(F("AT+LOG=Waiting for network: "));
  if (!sim800l->waitForNetwork())
  {
    Serial.println(" fail");
    delay(10000);
    return;
  }
  Serial.println(" success");
  if (sim800l->isNetworkConnected())
  {
    Serial.println("AT+LOG=Network connected");
  }
}

void sendSimGetRequest(char *server, char *resource, int timeout)
{
  // char *apn = "internet";
  // Serial.print(F("AT+LOG=Connecting to "));
  // Serial.print(apn);
  // if (!sim800l->gprsConnect(apn, "", ""))
  // {
  //   Serial.println(" fail");
  //   return;
  // }
  // Serial.println(" success");
  // if (sim800l->isGprsConnected())
  // {
  //   Serial.println("AT+LOG=GPRS connected");
  // }
  // Serial.print(F("AT+LOG=Local IP: "));
  // Serial.println(sim800l->getLocalIP());
  // Serial.print(F("AT+LOG=Connecting to "));
  // Serial.print(server);

  // HttpClient http(*sim800lclient, server, 80);
  // http.setTimeout(timeout);

  // http.connectionKeepAlive();

  // int err = http.get(resource);
  // if (err != 0)
  // {
  //   Serial.println(F("failed to connect"));
  //   sim800lclient->stop();
  //   delay(10000);
  //   return;
  // }

  // int status = http.responseStatusCode();
  // Serial.print(F("AT+LOG=Response status code: "));
  // Serial.println(status);
  // if (status < 100)
  // {
  //   Serial.println(F("AT+LOG=Request failed!"));
  //   sim800lclient->stop();
  //   http.stop();
  //   sim800l->gprsDisconnect();
  //   return;
  // }
  // if (!status)
  // {
  //   delay(10000);
  //   return;
  // }

  // Serial.println(F("Response Headers:"));
  // while (http.headerAvailable())
  // {
  //   String headerName = http.readHeaderName();
  //   String headerValue = http.readHeaderValue();
  //   Serial.println("    " + headerName + " : " + headerValue);
  // }

  // int length = http.contentLength();
  // if (length >= 0)
  // {
  //   Serial.print(F("Content length is: "));
  //   Serial.println(length);
  // }
  // if (http.isResponseChunked())
  // {
  //   Serial.println(F("The response is chunked"));
  // }
  // Serial.println(F("Response:"));
  // Serial.println(http.responseBody());

  // // Shutdown
  // http.stop();
  // Serial.println(F("Server disconnected"));
  // sim800l->gprsDisconnect();
}

void clearLCDLine(int line)
{
  lcd.setCursor(0, line);
  lcd.print("                    ");
}

void setup()
{
  readConfig();
  resetData();
  // Set test config
  loadDefaultSensorConfig();
  sensorConfig.DHT_ENABLED[0] = true;
  sensorConfig.windSpdPin[0] = A1;
  sensorConfig.windDirPin[0] = A0;
  saveSensorConfig();

  loadDefaultCommunicationConfig();
  comConfig.bleEnabled = true;
  comConfig.simEnabled = false;
  comConfig.wifiEnabled = true;
  comConfig.loraEnabled = true;
  comConfig.ethEnabled = false;
  saveComConfig();

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Weather Station");
  clearLCDLine(1);
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  
  // Initialize sensors
  Wire.begin();
  for (auto &sensor : dht)
  {
    if (sensorConfig.DHT_ENABLED[&sensor - &dht[0]])
    {
      sensor.begin();
    }
  }

  // Initialize Serial
  Serial.begin(9600);
  gpsSerial.begin(GPSBaud);
  // bleSerial.begin(9600);
  // loraSerial.begin(9600);

  // Initialize Communication
  if (comConfig.ethEnabled)
  {
    clearLCDLine(1);
    lcd.setCursor(0, 1);
    lcd.print("Initializing ETH");
    Serial.println(F("AT+LOG=Initializing Ethernet"));

    Ethernet.init(10); // Arduino Shield Ethernet
    if (Ethernet.begin(ethMac) == 0)
    {
      Serial.println(F("AT+LOG=Failed to configure Ethernet using DHCP"));
      // Check for Ethernet hardware present
      if (Ethernet.hardwareStatus() == EthernetNoHardware)
      {
        Serial.println(F("AT+LOG=Ethernet shield was not found.  Sorry, can't run without hardware. :("));
      }
      if (Ethernet.linkStatus() == LinkOFF)
      {
        Serial.println(F("AT+LOG=Ethernet cable is not connected."));
      }
      // Skip ETH initialization
    }
    else
    {
      Serial.println(F("AT+LOG=Ethernet initialized"));
      Serial.print(F("AT+LOG=IP address: "));
      Serial.println(Ethernet.localIP());
      deviceState.ethInitialized = true;
    }
  }
  delay(1000);

  if (comConfig.bleEnabled)
  {
    clearLCDLine(1);
    lcd.setCursor(0, 1);
    lcd.print("Initializing BLE");
    bleSerial.begin(9600);
    Serial.println(F("AT+LOG=Initializing BLE"));
    bleSerial.println("AT");
    delay(1000);
    readSerial(&bleSerial, 1000);
    Serial.print(F("AT+LOG=BLE response: "));
    Serial.println(serialAnswer);
    if (strstr(serialAnswer, "OK") == NULL)
    {
      Serial.println(F("AT+LOG=Failed to initialize BLE"));
    }
    else
    {
      Serial.println(F("AT+LOG=BLE initialized"));
      deviceState.bleInitialized = true;
    }
  }
  delay(1000);

  if (comConfig.simEnabled)
  {
    clearLCDLine(1);
    lcd.setCursor(0, 1);

    lcd.print("Initializing SIM");
    simSerial.begin(9600);
    
    Serial.println(F("AT+LOG=Initializing SIM"));
    setupSimModule();
    Serial.println(F("AT+LOG=SIM initialized"));
    deviceState.simInitialized = true;
  }
  delay(1000);

  if (comConfig.wifiEnabled)
  {
    clearLCDLine(1);
    lcd.setCursor(0, 1);
    lcd.print("Initializing WIFI");
    wifiSerial.begin(38400);
    Serial.println(F("AT+LOG=Initializing WIFI"));
    Serial.println(F("AT+LOG=Sending AT"));
    wifiSerial.print("AT\n");
    readSerial(&wifiSerial, 1000);
    Serial.print(F("AT+LOG=WIFI response: "));
    Serial.println(serialAnswer);

    // TODO: Implement WIFI module set SSID and password
    // wifi.print("AT+SSID=CGA2121_aaXyMrx\n");
    // delay(100);
    // while (wifi.available() == 0) {
    //   Serial.println("AT+LOG=Waiting for WIFI response");
    //   delay(1000);
    // }
    // serialAnswer = wifi.readString();
    // Serial.print("AT+LOG=WIFI response: ");
    // Serial.println(serialAnswer);

    // wifi.print("AT+PWD=dolna2a72\n");
    // delay(100);
    // while (wifi.available() == 0) {
    //   Serial.println("AT+LOG=Waiting for WIFI response");
    //   delay(1000);
    // }
    // serialAnswer = wifi.readString();
    // Serial.print("AT+LOG=WIFI response: ");
    // Serial.println(serialAnswer);

    Serial.println(F("AT+LOG=Sending AT+JOIN"));
    wifiSerial.print("AT+JOIN\n");
    readSerial(&wifiSerial, 10000);
    Serial.print(F("AT+LOG=WIFI response: "));
    Serial.println(serialAnswer);
    if (strstr(serialAnswer, "OK") == NULL && strstr(serialAnswer, "Connected") == NULL)
    {
      Serial.println(F("AT+LOG=Failed to join WIFI"));
    }
    else
    {
      Serial.println(F("AT+LOG=Joined WIFI"));
      deviceState.wifiInitialized = true;
    }
  }
  delay(1000);

  if (comConfig.loraEnabled)
  {
    clearLCDLine(1);
    lcd.setCursor(0, 1);
    lcd.print("Initializing LORA");
    loraSerial.begin(9600);
    Serial.println(F("AT+LOG=Initializing LORA"));
    loraSerial.print("AT\n");
    readSerial(&loraSerial, 1000);
    Serial.print(F("AT+LOG=LORA response: "));
    Serial.println(serialAnswer);
    if (strstr(serialAnswer, "OK") == NULL)
    {
      Serial.println(F("AT+LOG=Failed to initialize LORA"));
    }
    else
    {
      loraSerial.print("AT+JOIN\n");
      readSerial(&loraSerial, 1000);
      readTried = 1;
      while (readTried < 10 && strstr(serialAnswer, "Joined") == NULL && strstr(serialAnswer, "joined") == NULL && strstr(serialAnswer, "failed") == NULL)
      {
        Serial.print(F("AT+LOG=LORA response: "));
        Serial.println(serialAnswer);
        readSerial(&loraSerial, 3000);
        readTried++;
      }
      Serial.println(serialAnswer);
      if (strstr(serialAnswer, "failed") != NULL)
      {
        Serial.println(F("AT+LOG=LORA initialize failed!"));
      }
      if (strstr(serialAnswer, "Joined") != NULL || strstr(serialAnswer, "joined") != NULL)
      {
        Serial.println(F("AT+LOG=LORA initialized"));
        deviceState.loraInitialized = true;
      }
    }
  }

  lcd.setCursor(0, 1);
  lcd.print("BLE:" + String(deviceState.bleInitialized) + " SIM:" + String(deviceState.simInitialized) + " WIFI:" + String(deviceState.wifiInitialized));
  lcd.setCursor(0, 2);
  lcd.print("LORA:" + String(deviceState.loraInitialized) + " ETH:" + String(deviceState.ethInitialized));
  delay(5000);
  nextSendTime = millis() + 10000; // First send after 10 seconds
}

void loop()
{
  if (readSensorTime < millis())
  {
    readSensorTime = millis() + 1000;
    lcd.clear();
    for (int i = 0; i < maxWindSensor; i++)
    {
      if (sensorConfig.windDirPin[i] != -1)
      {
        windDirSensorVal[i] = analogRead(sensorConfig.windDirPin[i]);
        setWindDirection(i, windDirSensorVal[i]);
      }
    }
    for (int i = 0; i < maxWindSensor; i++)
    {
      if (sensorConfig.windSpdPin[i] != -1)
      {
        windSpdSensorVal[i] = analogRead(sensorConfig.windSpdPin[i]);
        windSpeed[i] = map(windSpdSensorVal[i], 0, 1023, 0, 300);
        windSpeed[i] /= 10;
      }
    }
    for (int i = 0; i < maxDhtSensor; i++)
    {
      if (sensorConfig.DHT_ENABLED[i])
      {
        humidity[i] = dht[i].readHumidity();
        temperature[i] = dht[i].readTemperature();
        if (isnan(humidity[i]) || isnan(temperature[i]))
        {
          Serial.println(F("AT+LOG=Failed to read from DHT sensor!"));
        }
      }
    }

    // Show data on LCD
    lcd.setCursor(0, 0);
    lcd.print("Sp: ");
    lcd.setCursor(0, 1);
    lcd.print("D: ");
    lcd.setCursor(0, 2);
    lcd.print("T: ");
    lcd.setCursor(0, 3);
    lcd.print("H: ");

    int col = 4;
    for (int i = 0; i < maxWindSensor; i++)
    {
      if (sensorConfig.windSpdPin[i] != -1)
      {
        lcd.setCursor(col, 0);
        lcd.print(String(windSpeed[i], 1));
        col += 4;
      }
    }
    col = 4;
    for (int i = 0; i < maxWindSensor; i++)
    {
      if (sensorConfig.windSpdPin[i] != -1)
      {
        lcd.setCursor(col, 1);
        lcd.print(windDirection[i]);
        col += 4;
      }
    }
    col = 4;
    for (int i = 0; i < maxDhtSensor; i++)
    {
      if (sensorConfig.DHT_ENABLED[i])
      {
        lcd.setCursor(col, 2);
        lcd.print(String(temperature[i], 1));
        col += 4;
      }
    }
    col = 4;
    for (int i = 0; i < maxDhtSensor; i++)
    {
      if (sensorConfig.DHT_ENABLED[i])
      {
        lcd.setCursor(col, 3);
        lcd.print(String(humidity[i], 1));
        col += 4;
      }
    }
  }

  // Handle Serial received data
  if (comConfig.ethEnabled)
  {
    // ether.packetLoop(ether.packetReceive());
  }
  if (serialAnswer[0] == 0)
  {
    if (Serial.available() > 0)
    {
      Serial.print(F("AT+LOG=Serial: "));
      readSerial(&Serial, 1000);
      messageSource = ATMessageSource::AT_Serial;
    }
  }
  if (comConfig.bleEnabled && serialAnswer[0] == 0)
  {
    if (bleSerial.available() > 0)
    {
      Serial.print(F("AT+LOG=BLE: "));
      readSerial(&bleSerial, 1000);
      messageSource = ATMessageSource::AT_BLE;
    }
  }
  // if (comConfig.simEnabled && serialAnswer[0] == 0)
  // {
  //   if (simSerial.available() > 0)
  //   {
  //     Serial.print(F("AT+LOG=SIM: "));
  //     readSerial(&simSerial, 1000);
  //     messageSource = ATMessageSource::AT_SIM;
  //   }
  // }
  if (comConfig.wifiEnabled && serialAnswer[0] == 0)
  {
    if (wifiSerial.available() > 0)
    {
      Serial.print(F("AT+LOG=WIFI: "));
      readSerial(&wifiSerial, 1000);
      messageSource = ATMessageSource::AT_WIFI;
    }
  }
  if (comConfig.loraEnabled && serialAnswer[0] == 0)
  {
    if (loraSerial.available() > 0)
    {
      Serial.print(F("AT+LOG=LORA: "));
      readSerial(&loraSerial, 1000);
      messageSource = ATMessageSource::AT_LORA;
    }
  }
  if (strlen(serialAnswer) > 0)
  {
    Serial.print(F("AT+LOG=Serial: "));
    Serial.println(serialAnswer);
    processATCommand();
    memset(serialAnswer, 0, sizeof(serialAnswer));
  }

  // Handle GPS data
  if (gpsSerial.available() > 0)
  {
    while (gpsSerial.available() > 0)
      gps.encode(gpsSerial.read());

    if (gps.location.isValid())
    {
      if (gps.location.lat() != 0 && gps.location.lng() != 0)
      {
        comConfig.lastLocation.lat = gps.location.lat();
        comConfig.lastLocation.lon = gps.location.lng();
        saveComConfig();
      }
    }
  }

  if (millis() >= nextSendTime)
  {
    nextSendTime = millis() + comConfig.sendInterval;
    sendData();
  }
  if (millis() >= loraSendTimeout && loraSending)
  {
    resetLoraData();
  }

  delay(50);
}