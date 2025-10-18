#include "LGFX_CrowPanel.h"
#include <Arduino.h>

#define SCREEN_BACKLIGHT_PIN 46

LGFX display;

void initBacklight() {
  ledcSetup(0, 5000, 8);
  ledcAttachPin(SCREEN_BACKLIGHT_PIN, 0);
  ledcWrite(0, 128); // Set to 50%
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("CrowPanel Hello World Starting...");

  // Power control for the display
  pinMode(40, OUTPUT);
  digitalWrite(40, LOW);

  // Power control for touch controller & I2C
  pinMode(1, OUTPUT);
  digitalWrite(1, HIGH);
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);

  display.init();
  display.setRotation(0);

  initBacklight();

  display.fillScreen(TFT_BLACK);

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(3);
  display.setCursor(40, 100);
  display.println("Hello");
  display.setCursor(40, 130);
  display.println("World!");
}

void loop() {
  static uint32_t last = 0;

  if (millis() - last > 1000) {
    last = millis();
    Serial.printf("Running ... uptime: %lu ms\n", millis());
  }
}
