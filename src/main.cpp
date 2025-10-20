#include "LGFX_CrowPanel.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#define SCREEN_BACKLIGHT_PIN 46
#define ENCODER_A_PIN 45
#define ENCODER_B_PIN 42
#define SWITCH_PIN 41
#define LED_PIN 48
#define LED_NUM 5
#define DEFAULT_LED_BRIGHTNESS 25

LGFX display;
Adafruit_NeoPixel ledStrip =
    Adafruit_NeoPixel(LED_NUM, LED_PIN, NEO_GRB + NEO_KHZ800);

volatile int encoderValue = 50; // Start at 50 (middle value)
volatile int lastEncoded = 0;

volatile bool buttonPressed = false;
volatile unsigned long lastButtonTime = 0;

/**
 * @brief the backlight of the display to 50% brightness
 */
void initBacklight() {
  ledcSetup(0, 5000, 8);
  ledcAttachPin(SCREEN_BACKLIGHT_PIN, 0);
  ledcWrite(0, 128); // Set to 50%
}

/**
 * @brief Interrupt handler for the rotary encoder
 */
void IRAM_ATTR handleRotaryEncoder() {
  int currentA = digitalRead(ENCODER_A_PIN);
  int currentB = digitalRead(ENCODER_B_PIN);

  // Combine the two pin readings into a single encoded value
  int encoded = (currentA << 1) | currentB;

  // Create a sum of previous and current readings
  int sum = (lastEncoded << 2) | encoded;

  // Determine direction based on the pattern (which pin changes first)
  // This implements a state machine for reliable encoder reading
  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) {
    encoderValue++; // Clockwise rotation
    if (encoderValue > 100)
      encoderValue = 100; // Cap at 100
  }
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) {
    encoderValue--; // Counter-clockwise rotation
    if (encoderValue < 0)
      encoderValue = 0; // Cap at 0
  }

  lastEncoded = encoded;
}

/**
 * @brief Interrupt handler for the button switch
 */
void IRAM_ATTR handleButton() {
  unsigned long now = millis();

  // Debounce for 200ms
  if (now - lastButtonTime > 200) {
    if (digitalRead(SWITCH_PIN) == LOW) {
      buttonPressed = true;
      lastButtonTime = now;
    }
  }
}

/**
 * @brief Displays the current rotary encoder / button values.
 */
void updateDisplay() {
  display.fillScreen(TFT_BLACK);

  // Draw title
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.setCursor(35, 30);
  display.println("ENCODER");
  display.setCursor(50, 50);
  display.println("VALUE");

  // Draw encoder value
  display.setTextSize(4);
  display.setTextColor(TFT_CYAN);
  display.setCursor(70, 90);
  display.printf("%3d", encoderValue);

  // Draw bar
  int barWidth = map(encoderValue, 0, 100, 0, 200);
  display.fillRect(20, 150, 200, 30, TFT_DARKGREY);
  display.fillRect(20, 150, barWidth, 30, TFT_GREEN);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

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

  // Set up encoder pins
  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // Attach interrupt handlers
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), handleRotaryEncoder,
                  CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), handleRotaryEncoder,
                  CHANGE);
  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), handleButton, FALLING);

  updateDisplay();
}

void loop() {
  static int lastDisplayValue = -1;

  // Update display when value changes
  if (encoderValue != lastDisplayValue) {
    updateDisplay();
    lastDisplayValue = encoderValue;

    Serial.print("Encoder Value: ");
    Serial.println(encoderValue);
  }

  // Update on button press
  if (buttonPressed) {
    buttonPressed = false;
    encoderValue = 50;
    Serial.println("Button pressed...resetting to 50");
    updateDisplay();
  }

  delay(10);
}
