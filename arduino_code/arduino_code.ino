#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"

int windSpdPin1 = A0;  // A0: Wind speed sensor output
int windDirPin1 = A1;  // A1: Wind direction sensor output
// A2-A3, A6-A7: Other sensors

// D0 - D5: UART
#define DHT_PIN 6  // D6: Temperature
#define DHT_TYPE DHT22
#define LED_PIN 7  // D7: Led
#define SELECT_BTN_PIN 8  // D8 - D9: Buttons
#define MOVING_BTN_PIN 9
// D10 - D13: SPI - Ethernet

int windSpdSensorVal = 0;
int windDirSensorVal = 0;
float windSpeed = 0;
float windDirection = 0;
float humidity= 0;
float temperature = 0;

bool selectingCat = 0;
bool selectedCat = 0;

LiquidCrystal_I2C lcd(0x27, 20, 4);
DHT dht(DHT_PIN, DHT_TYPE);

void setup() {
  // put your setup code here, to run once:
  Wire.begin();
  dht.begin();
  Serial.begin(9600);

  // Initialize LCD
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Welcome to");
  lcd.setCursor(0, 1);
  lcd.print("Weather Station");
  lcd.setCursor(0, 2);
  lcd.print("Monitor");

  // Initialize buttons
  pinMode(SELECT_BTN_PIN, INPUT_PULLUP);
  pinMode(MOVING_BTN_PIN, INPUT_PULLUP);
}

void loop() {
  lcd.clear();
  // put your main code here, to run repeatedly:
  windSpdSensorVal = analogRead(windSpdPin1);
  windSpeed = map(windSpdSensorVal, 0, 1023, 0, 300);
  windSpeed = windSpeed / 10;
  lcd.setCursor(0, 0);
  lcd.print("Speed: ");
  lcd.print(windSpeed);

  windDirSensorVal = analogRead(windDirPin1);
  windDirection = map(windDirSensorVal, 0, 1024, 0, 5);
  lcd.setCursor(0, 1);
  lcd.print("Direction: ");
  lcd.print(windDirection);

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
  }
  else {
    humidity = dht.readHumidity();
    lcd.setCursor(0, 2);
    lcd.print("Humidity: ");
    lcd.print(humidity);
    lcd.print("%");

    temperature = dht.readTemperature();
    lcd.setCursor(0, 3);
    lcd.print("Temperature: ");
    lcd.print(temperature);
    lcd.print(char(223));
    lcd.print("C");
  }
  delay(500);
}
