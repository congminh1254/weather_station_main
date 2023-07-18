#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);

int windSpdPin1 = A0;  // A0: Wind speed sensor output
int windDirPin1 = A1;  // A1: Wind direction sensor output
// A2-A3, A6-A7: Other sensors

// D0 - D5: UART
// D6: Temperature
// D7: Led
// D8 - D9: Buttons
// D10 - D13: SPI - Ethernet
int ledPin = 7;
int selectBtnPin = 8;
int movingBtnPin = 9;


int windSpdSensorVal = 0;
int windDirSensorVal = 0;
float windSpeed = 0;
float windDirection = 0;

bool selectingCat = 0;
bool selectedCat = 0;


void setup() {
  // put your setup code here, to run once:
  Wire.begin();
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
  pinMode(selectBtnPin, INPUT_PULLUP);
  pinMode(movingBtnPin, INPUT_PULLUP);
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

  delay(500);
}
