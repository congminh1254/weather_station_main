#define TINY_GSM_MODEM_SIM800
// #if !defined(TINY_GSM_RX_BUFFER)
// #define TINY_GSM_RX_BUFFER 300
// #endif

#include <SoftwareSerial.h>
#include <EtherCard.h>
#include <EEPROM.h>
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>


// D10 - D13: SPI - Ethernet
int bleRx = 5;
int bleTx = 4;
int simRx = 2;
int simTx = 3;
int simReset = 13;
int wifiRx = 6;
int wifiTx = 7;
int loraRx = 8;
int loraTx = 9;

SoftwareSerial ble(bleRx, bleTx);
SoftwareSerial sim(simRx, simTx);
SoftwareSerial wifi(wifiRx, wifiTx);
SoftwareSerial lora(loraRx, loraTx);

uint8_t Ethernet::buffer[300];
static uint8_t ethMac[] = {0x74, 0x69, 0x69, 0x2D, 0x30, 0x31};
char serialAnswer[128];

TinyGsm* sim800l;
TinyGsmClient* sim800lclient;
const char APN[] = "internet";

struct MasterConfig
{
  bool bleEnabled;
  bool simEnabled;
  bool wifiEnabled;
  bool loraEnabled;
  bool ethEnabled;

  String toString()
  {
    return String(bleEnabled) + String(simEnabled) + String(wifiEnabled) + String(loraEnabled) + String(ethEnabled);
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

MasterConfig masterConfig;
WifiConfig wifiConfig;
SimConfig simConfig;
LoraConfig loraConfig;

void readMasterConfig()
{
  EEPROM.get(0, masterConfig);
  EEPROM.get(sizeof(masterConfig), wifiConfig);
  EEPROM.get(sizeof(masterConfig) + sizeof(wifiConfig), simConfig);
  EEPROM.get(sizeof(masterConfig) + sizeof(wifiConfig) + sizeof(simConfig), loraConfig);
  loadDefaultConfig();
}

void loadDefaultConfig()
{
  if (strlen(wifiConfig.ssid) == 0 && strlen(wifiConfig.pwd) == 0)
  {
    strcpy(wifiConfig.ssid, "CGA2121_aaXyMrx");
    strcpy(wifiConfig.pwd, "dolna2a72");
  }
  if (strlen(simConfig.apn) == 0 && strlen(simConfig.user) == 0 && strlen(simConfig.pwd) == 0)
  {
    strcpy(simConfig.apn, "internet");
    strcpy(simConfig.user, "");
    strcpy(simConfig.pwd, "");
  }
  if (strlen(loraConfig.devEui) == 0 && strlen(loraConfig.appEui) == 0 && strlen(loraConfig.appKey) == 0)
  {
    strcpy(loraConfig.devEui, "6081F9F7B8151EE5");
    strcpy(loraConfig.appEui, "6081F9E9F6B003F9");
    strcpy(loraConfig.appKey, "55F5B1B2CE8DED19A3DB57FD976D9C05");
  }
  writeMasterConfig();
}

void writeMasterConfig()
{
  EEPROM.put(0, masterConfig);
  EEPROM.put(sizeof(masterConfig), wifiConfig);
  EEPROM.put(sizeof(masterConfig) + sizeof(wifiConfig), simConfig);
  EEPROM.put(sizeof(masterConfig) + sizeof(wifiConfig) + sizeof(simConfig), loraConfig);
}

void printMasterConfig()
{
  Serial.print(F("AT+LOG=Master config: "));
  Serial.println(masterConfig.toString());
}

void readSoftSerial(SoftwareSerial *serial, int timeout)
{
  unsigned long readTimeout = millis() + timeout;
  memset(serialAnswer, 0, sizeof(serialAnswer));

  // Read until newline or timeout
  while (millis() < readTimeout)
  {
    if (serial->available() > 0)
    {
      char ch = serial->read();
      if (ch == '\n')
      {
        break;
      }
      else
      {
        serialAnswer[strlen(serialAnswer)] = ch;
      }
    }
    delay(1);
  }
  if (millis() >= readTimeout)
  {
    Serial.println(F("AT+LOG=Timeout while reading from soft serial"));
  }
}

void setupSimModule()
{
    sim800l = new TinyGsm(sim);
    sim800lclient = new TinyGsmClient(*sim800l);

    sim800l->init();
    Serial.print(F("Modem Info: "));
    Serial.println(sim800l->getModemInfo());
    Serial.print(F("Modem Name: "));
    Serial.println(sim800l->getModemName());
    Serial.print(F("Waiting for network: "));
    if (!sim800l->waitForNetwork()) {
      Serial.println(" fail");
      delay(10000);
      return;
    }
    Serial.println(" success");
    if (sim800l->isNetworkConnected()) { Serial.println("Network connected"); }
}

void sendSimGetRequest(char* server, char* resource, int timeout) {
  char* apn = "internet";
  Serial.print(F("Connecting to "));
  Serial.print(apn);
  if (!sim800l->gprsConnect(apn, "", ""))
  {
     Serial.println(" fail");
     return;
  }
  Serial.println(" success");
  if (sim800l->isGprsConnected()) { Serial.println("GPRS connected"); }
  Serial.print(F("Local IP: "));
  Serial.println(sim800l->getLocalIP());
  Serial.print(F("Connecting to "));
  Serial.print(server);

  HttpClient http(*sim800lclient, server, 80);
  http.setTimeout(timeout);

  http.connectionKeepAlive();

  int err = http.get(resource);
  if (err != 0) {
    Serial.println(F("failed to connect"));
    sim800lclient->stop();
    delay(10000);
    return;
  }

  int status = http.responseStatusCode();
  Serial.print(F("Response status code: "));
  Serial.println(status);
  if (status < 100) {
    Serial.println(F("Request failed!"));
    sim800lclient->stop();
    http.stop();
    sim800l->gprsDisconnect();
    return;
  }
  if (!status) {
    delay(10000);
    return;
  }

  Serial.println(F("Response Headers:"));
  while (http.headerAvailable()) {
    String headerName  = http.readHeaderName();
    String headerValue = http.readHeaderValue();
    Serial.println("    " + headerName + " : " + headerValue);
  }

  int length = http.contentLength();
  if (length >= 0) {
    Serial.print(F("Content length is: "));
    Serial.println(length);
  }
  if (http.isResponseChunked()) {
    Serial.println(F("The response is chunked"));
  }
  Serial.println(F("Response:"));
  Serial.println(http.responseBody());

  // Shutdown
  http.stop();
  Serial.println(F("Server disconnected"));
  sim800l->gprsDisconnect();
}

void setup()
{
  Serial.begin(9600);
  readMasterConfig();
  masterConfig.loraEnabled = true;
  masterConfig.wifiEnabled = true;
  masterConfig.ethEnabled = true;
  masterConfig.bleEnabled = true;
  masterConfig.simEnabled = true;
  printMasterConfig();

  if (masterConfig.ethEnabled)
  {
    Serial.println(F("AT+LOG=Initializing Ethernet"));
    // if (ether.begin(sizeof Ethernet::buffer, ethMac) == 0)
    //   Serial.println(F("AT+LOG=Failed to access Ethernet controller"));
    // if (!ether.dhcpSetup())
    //   Serial.println(F("AT+LOG=Ethernet DHCP failed"));
    ether.printIp("AT+LOG=IP:  ", ether.myip);
    ether.printIp("AT+LOG=GW:  ", ether.gwip);
    ether.printIp("AT+LOG=DNS: ", ether.dnsip);
    Serial.println(F("AT+LOG=Ethernet initialized"));
  }
  delay(1000);

  if (masterConfig.bleEnabled)
  {
    ble.begin(9600);
    Serial.println(F("AT+LOG=Initializing BLE"));
    ble.println("AT");
    delay(1000);
    readSoftSerial(&ble, 1000);
    Serial.print(F("AT+LOG=BLE response: "));
    Serial.println(serialAnswer);
    if (strstr(serialAnswer, "OK") == NULL) {
      Serial.println(F("AT+LOG=Failed to initialize BLE"));
    }
    else {
      Serial.println(F("AT+LOG=BLE initialized"));
    }
  }
  delay(1000);

  if (masterConfig.simEnabled)
  {
    sim.begin(9600);
    Serial.println(F("AT+LOG=Initializing SIM"));
    setupSimModule();
    sendSimGetRequest("postman-echo.com", "/get", 10000);

    Serial.println(F("AT+LOG=SIM initialized"));
  }
  delay(1000);

  if (masterConfig.wifiEnabled)
  {
    wifi.begin(9600);
    Serial.println(F("AT+LOG=Initializing WIFI"));
    Serial.println(F("AT+LOG=Sending AT"));
    wifi.print("AT\n");
    readSoftSerial(&wifi, 1000);
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
    wifi.print("AT+JOIN\n");
    readSoftSerial(&wifi, 10000);
    Serial.print(F("AT+LOG=WIFI response: "));
    Serial.println(serialAnswer);
    if (strstr(serialAnswer, "OK") == NULL && strstr(serialAnswer, "Connected") == NULL)
    {
      Serial.println(F("AT+LOG=Failed to join WIFI"));
    }
    else
    {
      Serial.println(F("AT+LOG=Joined WIFI"));
    }
  }
  delay(1000);

  if (masterConfig.loraEnabled)
  {
    lora.begin(9600);
    Serial.println("AT+LOG=Initializing LORA");
    lora.print("AT\n");
    readSoftSerial(&lora, 1000);
    Serial.print("AT+LOG=LORA response: ");
    Serial.println(serialAnswer);
    if (strstr(serialAnswer, "OK") == NULL)
    {
      Serial.println(F("AT+LOG=Failed to initialize LORA"));
    }
    else
    {
      lora.print("AT+JOIN\n");
      readSoftSerial(&lora, 1000);
      while (strstr(serialAnswer, "Joined") == NULL && strstr(serialAnswer, "joined") == NULL)
      {
        Serial.print(F("AT+LOG=LORA response: "));
        Serial.println(serialAnswer);
        readSoftSerial(&lora, 1000);
      }
      Serial.println(serialAnswer);
      Serial.println(F("AT+LOG=LORA initialized"));
    }
  }
  delay(1000);
  Serial.println(F("AT+LOG=Setup complete"));
}

void loop()
{
  // lcd.clear();

  // windSpdSensorVal = analogRead(windSpdPin1);
  // windSpeed = map(windSpdSensorVal, 0, 1023, 0, 300);
  // windSpeed = windSpeed / 10;
  // lcd.setCursor(0, 0);
  // lcd.print("Speed: ");
  // lcd.print(windSpeed);

  // windDirSensorVal = analogRead(windDirPin1);
  // windDirection = map(windDirSensorVal, 0, 1024, 0, 5);
  // lcd.setCursor(0, 1);
  // lcd.print("Direction: ");
  // lcd.print(windDirection);

  // if (isnan(humidity) || isnan(temperature)) {
  //   Serial.println("Failed to read from DHT sensor!");
  // }
  // else {
  //   humidity = dht.readHumidity();
  //   lcd.setCursor(0, 2);
  //   lcd.print("Humidity: ");
  //   lcd.print(humidity);
  //   lcd.print("%");

  //   temperature = dht.readTemperature();
  //   lcd.setCursor(0, 3);
  //   lcd.print("Temperature: ");
  //   lcd.print(temperature);
  //   lcd.print(char(223));
  //   lcd.print("C");
  // }
  // delay(500);

  // Khi máy tính gửi dữ liệu cho mình
  // if (Serial.available() > 0) {
  //   Serial.print(F("May tinh gui: "));
  //   while (Serial.available() > 0) {// in hết nội dung mà máy tính gửi cho mình, đồng thời gửi cho arduino thứ 2
  //     char ch = Serial.read(); //đọc ký tự đầu tiên trogn buffer
  //     Serial.write(ch); //xuất ra monitor máy tính
  //     wifi.write(ch); //gửi dữ liệu cho Arduino thứ 2
  //     delay(3);
  //   }
  //   Serial.println();
  // }

  // if (wifi.available() > 0) {
  //   Serial.println(F("Serial thu 2 gui gia tri: "));
  //   //đọc giá trị từ Arduino nếu có
  //   delay(20);
  //   while (wifi.available() > 0) {
  //     char ch = wifi.read(); //đọc
  //     Serial.write(ch); //xuất ra monitor
  //     delay(3);
  //   }
  //   Serial.println();
  // }

  // Serial.write("AT\r\n");
  // delay(1000);
}
