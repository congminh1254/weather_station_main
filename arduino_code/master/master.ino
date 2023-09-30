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
char serialAnswer[150];
unsigned long readTimeout = 0;

TinyGsm *sim800l;
TinyGsmClient *sim800lclient;
const char APN[] = "internet";

struct MasterConfig
{
  bool bleEnabled;
  bool simEnabled;
  bool wifiEnabled;
  bool loraEnabled;
  bool ethEnabled;
  int stationId;

  String toString()
  {
    return "Station " + String(stationId) + " " + String(bleEnabled) + String(simEnabled) + String(wifiEnabled) + String(loraEnabled) + String(ethEnabled);
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
  if (masterConfig.stationId < 1000)
  {
    masterConfig.stationId = random(1000, 9999);
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

void readSerial(int timeout)
{
  readTimeout = millis() + timeout;
  memset(serialAnswer, 0, sizeof(serialAnswer));

  // Read until newline or timeout
  while (millis() < readTimeout)
  {
    if (Serial.available() > 0)
    {
      char ch = Serial.read();
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
    Serial.println(F("AT+LOG=Timeout while reading from serial"));
  }
  trimSerialAnswer();
}

void readSoftSerial(SoftwareSerial *serial, int timeout)
{
  readTimeout = millis() + timeout;
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
  trimSerialAnswer();
}

void setupSimModule()
{
  sim800l = new TinyGsm(sim);
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
  char *apn = "internet";
  Serial.print(F("AT+LOG=Connecting to "));
  Serial.print(apn);
  if (!sim800l->gprsConnect(apn, "", ""))
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
  Serial.print(server);

  HttpClient http(*sim800lclient, server, 80);
  http.setTimeout(timeout);

  http.connectionKeepAlive();

  int err = http.get(resource);
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

  Serial.println(F("Response Headers:"));
  while (http.headerAvailable())
  {
    String headerName = http.readHeaderName();
    String headerValue = http.readHeaderValue();
    Serial.println("    " + headerName + " : " + headerValue);
  }

  int length = http.contentLength();
  if (length >= 0)
  {
    Serial.print(F("Content length is: "));
    Serial.println(length);
  }
  if (http.isResponseChunked())
  {
    Serial.println(F("The response is chunked"));
  }
  Serial.println(F("Response:"));
  Serial.println(http.responseBody());

  // Shutdown
  http.stop();
  Serial.println(F("Server disconnected"));
  sim800l->gprsDisconnect();
}

const int MAX_LORA_LENGTH = 36;
void sendLoraData(const char *data)
{
  // Lora package can send upto 51 bytes,
  // so we need to split the message into multiple packages
  // with format: x:y:deviceId:z with x is the current position of message,
  // y is the total length of message, z is the message content
  // length of each message is upto 36 bytes

  int currentPos = 0;
  int totalLength = strlen(data);
  int timeTried = 0;
  char buffer[totalLength + 1];
  memcpy(buffer, data, totalLength);
  buffer[totalLength] = '\0';

  while (currentPos < totalLength)
  {
    if (currentPos > 0)
    {
      memset(serialAnswer, 0, sizeof(serialAnswer));
      while (lora.available())
        lora.read();
      delay(1000);
    }
    Serial.print(F("AT+LOG=Sending :"));
    lora.print("AT+CMSG=\"");
    lora.print(currentPos);
    lora.print(":");
    lora.print(totalLength);
    lora.print(":");
    lora.print(masterConfig.stationId);
    lora.print(":");

    for (int i = currentPos; i < currentPos + MAX_LORA_LENGTH; i++)
    {
      if (buffer[i] == '"')
        buffer[i] = '\'';
      if (i < totalLength)
      {
        lora.print(buffer[i]);
        Serial.print(buffer[i]);
      }
      else
      {
        break;
      }
    }
    currentPos += MAX_LORA_LENGTH;

    lora.println("\"");
    Serial.println("");

    timeTried = 0;
    while ((strstr(serialAnswer, "Done") == NULL && strstr(serialAnswer, "ERROR") == NULL && strstr(serialAnswer, "error") == NULL) && timeTried < 10)
    {
      readSoftSerial(&lora, 3000);
      Serial.print(F("AT+LOG=LORA response: "));
      Serial.println(serialAnswer);
      timeTried++;
    }
    // if (strstr(serialAnswer, "Done") == NULL == NULL)
    // {
    //   Serial.println(F("AT+LOG=Failed to send LORA data"));
    // }
    // else
    // {
    //   Serial.println(F("AT+LOG=LORA data sent"));
    // }
  }
}

void processATCommand()
{
  // AT+DATA=... : Data from Slave, need to send to server
  // AT+LOG=... : Log from Slave, do nothing

  if (strncmp(serialAnswer, "AT+DATA=", 8) == 0)
  {
    if (masterConfig.ethEnabled)
    {
      // Serial.println(F("AT+LOG=Sending data via Ethernet"));
      // ether.packetLoop(ether.packetReceive());
      // ether.httpPost("/data", dataBuffer, "application/json");
      // Serial.println(F("AT+LOG=Data sent to server"));
    }
    if (masterConfig.simEnabled)
    {
      // Serial.println(F("AT+LOG=Sending data via SIM"));
      // sendSimGetRequest("postman-echo.com", "/post", 10000);
      // Serial.println(F("AT+LOG=Data sent to server"));
    }
    if (masterConfig.wifiEnabled)
    {
      // Serial.println(F("AT+LOG=Sending data via WIFI"));
      // sendWifiPostRequest("postman-echo.com", "/post", 10000);
      // Serial.println(F("AT+LOG=Data sent to server"));
    }
    if (masterConfig.loraEnabled)
    {
      Serial.println(F("AT+LOG=Sending data via LORA"));
      sendLoraData(serialAnswer + 8);
      Serial.println(F("AT+LOG=Data sent to server"));
    }
    if (masterConfig.bleEnabled)
    {
      // Serial.println(F("AT+LOG=Sending data via BLE"));
      // sendBlePostRequest("postman-echo.com", "/post", 10000);
      // Serial.println(F("AT+LOG=Data sent to server"));
    }
  }
  else if (strncmp(serialAnswer, "AT+LOG=", 7) == 0)
  {
  }
}

void setup()
{
  Serial.begin(9600);
  readMasterConfig();
  masterConfig.loraEnabled = true;
  masterConfig.wifiEnabled = true;
  masterConfig.ethEnabled = false;
  masterConfig.bleEnabled = true;
  masterConfig.simEnabled = false;
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
    if (strstr(serialAnswer, "OK") == NULL)
    {
      Serial.println(F("AT+LOG=Failed to initialize BLE"));
    }
    else
    {
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
    Serial.println(F("AT+LOG=Initializing LORA"));
    lora.print("AT\n");
    readSoftSerial(&lora, 1000);
    Serial.print(F("AT+LOG=LORA response: "));
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
  memset(serialAnswer, 0, sizeof(serialAnswer));
  delay(1000);
  Serial.println(F("AT+LOG=Setup complete"));
}

void loop()
{
  if (masterConfig.ethEnabled)
  {
    ether.packetLoop(ether.packetReceive());
  }
  if (serialAnswer[0] == 0)
  {
    if (Serial.available() > 0)
    {
      Serial.print(F("AT+LOG=Serial: "));
      readSerial(1000);
    }
  }
  if (masterConfig.bleEnabled && serialAnswer[0] == 0)
  {
    if (ble.available() > 0)
    {
      Serial.print(F("AT+LOG=BLE: "));
      readSoftSerial(&ble, 1000);
    }
  }
  if (masterConfig.simEnabled && serialAnswer[0] == 0)
  {
    if (sim.available() > 0)
    {
      Serial.print(F("AT+LOG=SIM: "));
      readSoftSerial(&sim, 1000);
    }
  }
  if (masterConfig.wifiEnabled && serialAnswer[0] == 0)
  {
    if (wifi.available() > 0)
    {
      Serial.print(F("AT+LOG=WIFI: "));
      readSoftSerial(&wifi, 1000);
    }
  }
  if (masterConfig.loraEnabled && serialAnswer[0] == 0)
  {
    if (lora.available() > 0)
    {
      Serial.print(F("AT+LOG=LORA: "));
      readSoftSerial(&lora, 1000);
    }
  }
  if (strlen(serialAnswer) > 0)
  {
    Serial.print(F("AT+LOG=Serial: "));
    Serial.println(serialAnswer);
    processATCommand();
    memset(serialAnswer, 0, sizeof(serialAnswer));
  }
  delay(10);
}
