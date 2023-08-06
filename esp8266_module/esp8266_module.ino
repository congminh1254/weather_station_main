#include <Wire.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#define EEPROM_SIZE 512
#define SSID_MAX_LENGTH 32
#define PWD_MAX_LENGTH 64
#define COMMAND_TIMEOUT 50 // Timeout in milliseconds
#define LED_BLINK_INTERVAL 500 // Blink interval for network not connected (in milliseconds)

byte ssidLength = 0;
byte pwdLength = 0;
char ssid[SSID_MAX_LENGTH];
char pwd[PWD_MAX_LENGTH];

unsigned long commandStartTime = 0;
unsigned long blinkStartTime = 0;

HTTPClient http;
std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT); // Set LED_BUILTIN pin as OUTPUT
  Serial.begin(115200);
  Wire.begin(4, 5); // Initialize I2C communication (SDA - GPIO4, SCL - GPIO5)
  WiFi.hostname("WeatherStation");
  client->setInsecure();

  readSettingsFromEEPROM(); // Read the saved settings from EEPROM
}

void loop()
{
  // Check if there is any I2C data available
  if (Wire.available())
  {
    char command[1024];
    byte commandLength = 0;
    commandStartTime = millis(); // Reset the command start time
    while (commandLength < 1024 && millis() - commandStartTime < COMMAND_TIMEOUT)
    {
      if (!Wire.available())
      {
        delay(1); // Wait for more I2C data to arrive
        continue;
      }
      char c = Wire.read(); // Read the incoming I2C data
      if (c == '\n')
        break; // Stop reading if newline character is received
      command[commandLength++] = c;
    }
    command[commandLength] = '\0';

    // Parse and execute the received command
    executeCommand(command);
  }

  // Check if there is any Serial data available
  if (Serial.available())
  {
    char command[1024];
    byte commandLength = 0;
    commandStartTime = millis(); // Reset the command start time
    while (commandLength < 1024 && millis() - commandStartTime < COMMAND_TIMEOUT)
    {
      if (!Serial.available())
      {
        delay(1); // Wait for more Serial data to arrive
        continue;
      }
      char c = Serial.read(); // Read the incoming Serial data
      if (c == '\n')
        break; // Stop reading if newline character is received
      command[commandLength++] = c;
    }
    command[commandLength] = '\0';

    // Parse and execute the received command
    executeCommand(command);
  }

  // Check network connection and blink LED if not connected
  if (!(WiFi.status() == WL_CONNECTED) && millis() - blinkStartTime >= LED_BLINK_INTERVAL)
  {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); // Toggle LED state
    blinkStartTime = millis(); // Reset the blink start time
  } else if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, LOW); // Turn on LED when connected
  }
}

void executeCommand(const char *command)
{
  if (strcmp(command, "AT") == 0)
  {
    // Return status of the module
    sendResponse("OK");
  }
  else if (strncmp(command, "AT+SSID=", 8) == 0)
  {
    // Set the SSID into settings
    setSSID(command + 8);
    sendResponse("OK");
  }
  else if (strcmp(command, "AT+SSID") == 0)
  {
    // Return current SSID in settings
    sendResponse(ssid);
  }
  else if (strncmp(command, "AT+PWD=", 7) == 0)
  {
    // Set the Wi-Fi password
    setPassword(command + 7);
    sendResponse("OK");
  }
  else if (strcmp(command, "AT+PWD") == 0)
  {
    // Return current password in settings
    sendResponse(pwd);
  }
  else if (strcmp(command, "AT+JOIN") == 0)
  {
    // Request to join the Wi-Fi network
    joinWiFi();
  }
  else if (strcmp(command, "AT+LEAVE") == 0)
  {
    // Request to leave the Wi-Fi network
    WiFi.disconnect();
    sendResponse("OK");
  }
  else if (strcmp(command, "AT+IP") == 0)
  {
    // Return the IP address of the module
    sendResponse(WiFi.localIP().toString().c_str());
  }
  else if (strcmp(command, "AT+INTERNET") == 0)
  {
    // Return if there is an internet connection or not
    if (checkInternetConnection())
      sendResponse("Connected");
    else
      sendResponse("No internet connection");
  }
  else if (strcmp(command, "AT+NEWREQ") == 0)
  {
    // Start a new HTTP request
    http = HTTPClient();
    sendResponse("OK");
  }
  else if (strncmp(command, "AT+URL=", 7) == 0)
  {
    // Set the URL for the HTTP request
    String url = String(command + 7);
    http.begin(*client, url);
    sendResponse("OK");
  }
  else if (strncmp(command, "AT+HEADER=", 10) == 0) {
    // Add a header to the HTTP request
    String headerName = String(command + 10);
    int colonIndex = headerName.indexOf(':');
    if (colonIndex == -1) {
      sendResponse("Error: Invalid header");
      return;
    }
    String headerValue = headerName.substring(colonIndex + 1);
    headerName = headerName.substring(0, colonIndex);
    http.addHeader(headerName.c_str(), headerValue.c_str());
  }
  else if (strcmp(command, "AT+GET") == 0)
  {
    // Send a GET request
    int httpCode = http.GET();
    sendResponse(String(httpCode).c_str());
  }
  else if (strncmp(command, "AT+POST=", 8) == 0)
  {
    // Send a POST request
    int httpCode = http.POST(command + 8);
    sendResponse(String(httpCode).c_str());
  }
  else if (strcmp(command, "AT+POST") == 0)
  {
    // Send a POST request
    int httpCode = http.POST("");
    sendResponse(String(httpCode).c_str());
  }
  else if (strcmp(command, "AT+READ") == 0)
  {
    // Read the response from the HTTP request
    String response = http.getString();
    sendResponse(response.c_str());
  }
  else if (strcmp(command, "AT+ENDREQ") == 0)
  {
    // End the HTTP request
    http.end();
    sendResponse("OK");
  }
  else
  {
    // Unknown command
    sendResponse("Error: Unknown command");
  }
}

void setSSID(const char *newSSID)
{
  ssidLength = strlen(newSSID);
  if (ssidLength > SSID_MAX_LENGTH - 1)
    ssidLength = SSID_MAX_LENGTH - 1;

  strncpy(ssid, newSSID, ssidLength);
  ssid[ssidLength] = '\0';

  saveSettingsToEEPROM();
}

void setPassword(const char *newPwd)
{
  pwdLength = strlen(newPwd);
  if (pwdLength > PWD_MAX_LENGTH - 1)
    pwdLength = PWD_MAX_LENGTH - 1;

  strncpy(pwd, newPwd, pwdLength);
  pwd[pwdLength] = '\0';

  saveSettingsToEEPROM();
}

void saveSettingsToEEPROM()
{
  EEPROM.begin(EEPROM_SIZE);

  // Save SSID
  for (byte i = 0; i < ssidLength; i++)
    EEPROM.write(i, ssid[i]);
  EEPROM.write(ssidLength, '\0');

  // Save password
  for (byte i = 0; i < pwdLength; i++)
    EEPROM.write(SSID_MAX_LENGTH + i, pwd[i]);
  EEPROM.write(SSID_MAX_LENGTH + pwdLength, '\0');

  EEPROM.commit();
  EEPROM.end();
}

void readSettingsFromEEPROM()
{
  EEPROM.begin(EEPROM_SIZE);

  // Read SSID
  for (byte i = 0; i < SSID_MAX_LENGTH; i++)
  {
    char c = EEPROM.read(i);
    ssid[i] = c;
    if (c == '\0')
      break;
  }

  // Read password
  for (byte i = 0; i < PWD_MAX_LENGTH; i++)
  {
    char c = EEPROM.read(SSID_MAX_LENGTH + i);
    pwd[i] = c;
    if (c == '\0')
      break;
  }

  EEPROM.end();
}

void sendResponse(const char *response)
{
  Wire.beginTransmission(8); // Address of the I2C master
  Wire.write(response);
  Wire.endTransmission();

  Serial.println(response);
}

bool joinWiFi()
{
  WiFi.begin(ssid, pwd); // Connect to Wi-Fi network using stored SSID and password

  int timeout = 50; // Timeout in seconds
  while (WiFi.status() != WL_CONNECTED && timeout > 0)
  {
    digitalWrite(LED_BUILTIN, HIGH); // Turn on LED
    delay(100);
    digitalWrite(LED_BUILTIN, LOW); // Turn off LED
    delay(100);

    timeout--;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    // Connected to Wi-Fi network
    digitalWrite(LED_BUILTIN, LOW); // Turn on LED when connected
    sendResponse("Connected");
    return true;
  }
  else
  {
    // Failed to connect
    digitalWrite(LED_BUILTIN, HIGH); // Turn off LED when not connected

    // Write the error code to I2C and Serial interfaces
    switch (WiFi.status())
    {
      case WL_NO_SSID_AVAIL:
        sendResponse("Error: WL_NO_SSID_AVAIL");
        break;
      case WL_SCAN_COMPLETED:
        sendResponse("Error: WL_SCAN_COMPLETED");
        break;
      case WL_CONNECT_FAILED:
        sendResponse("Error: WL_CONNECT_FAILED");
        break;
      case WL_CONNECTION_LOST:
        sendResponse("Error: WL_CONNECTION_LOST");
        break;
      case WL_WRONG_PASSWORD:
        sendResponse("Error: WL_WRONG_PASSWORD");
        break;
      case WL_DISCONNECTED:
        sendResponse("Error: WL_DISCONNECTED");
        break;
      default:
        sendResponse("Error: Unknown error");
        break;
    }

    return false;
  }
}

bool checkInternetConnection()
{
  // Code to check internet connection status
  // Replace with your implementation
  if (WiFi.status() != WL_CONNECTED)
    return false;
  http = HTTPClient();
  http.begin(*client, "https://www.google.com");
  int httpCode = http.GET();
  http.end();

  if (httpCode < 0)
    return false;
  return true;
}
