#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <SoftwareSerial.h>


int windSpdPin1 = A0;  // A0: Wind speed sensor output
int windDirPin1 = A1;  // A1: Wind direction sensor output
// A2-A3, A6-A7: Other sensors

// D0 - D5: UART
#define DHT_PIN 6  // D6: Temperature
#define DHT_TYPE DHT22
#define LED_PIN 7  // D7: Led
#define SELECT_BTN_PIN 2  // D8 - D9: Buttons
#define MOVING_BTN_PIN 3
// D10 - D13: SPI - Ethernet

int windSpdSensorVal = 0;
int windDirSensorVal = 0;

int bleRx = 6;
int bleTx = 7;
int simRx = 8;
int simTx = 9;

SoftwareSerial ble(bleRx, bleTx);
SoftwareSerial sim(simRx, simTx);

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
  ble.begin(9600);
  sim.begin(9600);

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
  if (Serial.available() > 0) {
    Serial.print("May tinh gui: ");
    while (Serial.available() > 0) {// in hết nội dung mà máy tính gửi cho mình, đồng thời gửi cho arduino thứ 2  
      char ch = Serial.read(); //đọc ký tự đầu tiên trogn buffer
      Serial.write(ch); //xuất ra monitor máy tính
      sim.write(ch); //gửi dữ liệu cho Arduino thứ 2
      delay(3);
    }    
    Serial.println();
  }
  
  if (sim.available() > 0) {
    Serial.println("Serial thu 2 gui gia tri: ");
    //đọc giá trị từ Arduino nếu có
    delay(20);
    while (sim.available() > 0) {
      char ch = sim.read(); //đọc
      Serial.write(ch); //xuất ra monitor
      delay(3);
    }
    Serial.println();
  }

  // Serial.write("AT\r\n");
  // delay(1000);
}
