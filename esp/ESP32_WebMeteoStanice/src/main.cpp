#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>


TFT_eSPI tft = TFT_eSPI();

#define tftCSpin 5

const char* apSsid = "esp32-meteostanice";
const char* apPass = "Admin123";


void setup() {
  Serial.begin(9600);
  Serial.println("Serial ready!");
  const char * welcome = "TEST!!!";
  tft.begin();
  tft.fillScreen(TFT_PINK);
  tft.setTextSize(2);
  tft.setTextFont(1);
  tft.setTextColor(TFT_BLUE);
  tft.print(welcome);
  delay(1000);

  // put your setup code here, to run once:
}

void loop() {
  // put your main code here, to run repeatedly:
  int number = random(255);
  String line1 = String("ahoj kadime dneska sem to po tydnu zapojyl!!!!!");
  String line2 = String("tadi mas nahodni cisco: ") + String(number);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0);
  tft.setTextSize(2);
  tft.setTextFont(2);
  tft.setTextColor(TFT_GREEN);
  tft.print(line1);
  tft.setCursor(0,10);
  tft.setTextSize(2);
  tft.setTextFont(3);
  tft.setTextColor(TFT_RED);
  tft.print(line2);
  delay(1000);
}