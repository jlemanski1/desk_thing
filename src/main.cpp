#include "lgfx_crow_panel.h"
// #include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <WiFiManager.h>
#include "credentials.h"
#include "debug.h"
#include "spotify_client.h"
#include <Arduino.h>



#define SCREEN_BACKLIGHT_PIN 46
#define ENCODER_A_PIN 45
#define ENCODER_B_PIN 42
#define SWITCH_PIN 41
#define LED_PIN 48
#define LED_NUM 5
#define DEFAULT_LED_BRIGHTNESS 25

LGFX display;
SpotifyClient spotify(SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, SPOTIFY_REFRESH_TOKEN);
// Adafruit_NeoPixel ledStrip = Adafruit_NeoPixel(LED_NUM, LED_PIN, NEO_GRB + NEO_KHZ800);

volatile int encoderValue = 50;
volatile int lastEncoded = 0;

volatile bool buttonPressed = false;
volatile unsigned long lastButtonTime = 0;

// Update interval
unsigned long lastPlaybackUpdate = 0;
const unsigned long playbackUpdateInterval = 2000;

void connectWiFi() {
  DebugPrintln("Starting WiFi configuration...");

  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE);
  display.setTextSize(2);
  display.setCursor(40, 90);
  display.println("WiFi Setup");
  display.setTextSize(1);
  display.setCursor(30, 120);
  display.println("Connect phone to:");
  display.setTextSize(2);
  display.setTextColor(TFT_CYAN);
  display.setCursor(10, 140);
  display.println("DeskThing");
  display.setTextSize(1);
  display.setTextColor(TFT_YELLOW);
  display.setCursor(30, 170);
  display.println("Password: ESP32-DeskThing");

  // Create WiFiManager instance
  WiFiManager wifiManager;

  // Set timeout for portal (3mins)
  wifiManager.setConfigPortalTimeout(180);

  wifiManager.setAPCallback([](WiFiManager* myWiFiManager) {
    DebugPrintln("==============================");
    DebugPrintln("Entered WiFi Config Mode");
    DebugPrintln("==============================");
    DebugPrintln("Connect to WiFi network:");
    DebugPrintln("  SSID: " + String(myWiFiManager->getConfigPortalSSID()));
    DebugPrintln("  Password: ESP32-DeskThing");
    DebugPrintln("Then open: http://192.168.4.1");
    DebugPrintln("==============================");
    });

  // Try to connect with saved credentials, or start config portal
  // Network name: "DeskThing"
  // Password: "ESP32-DeskThing"
  bool connected = wifiManager.autoConnect("DeskThing", "ESP32-DeskThing");

  if (connected) {
    DebugPrintln("\nWiFi Connected!");
    DebugPrint("SSID: ");
    DebugPrintln(WiFi.SSID());
    DebugPrint("IP Address: ");
    DebugPrintln(WiFi.localIP());

    display.fillScreen(TFT_BLACK);
    display.setTextSize(2);
    display.setTextColor(TFT_GREEN);
    display.setCursor(40, 110);
    display.println("WiFi OK!");
    delay(1000);
  }
  else {
    DebugPrintln("\nWiFi connection failed or timeout");
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_RED);
    display.setTextSize(2);
    display.setCursor(30, 100);
    display.println("WiFi Failed");
    display.setTextSize(1);
    display.setCursor(30, 130);
    display.println("Check connection");
    display.setCursor(40, 150);
    display.println("Restarting...");
    delay(3000);
    ESP.restart();
  }
}

String formatTime(int milliseconds) {
  int seconds = milliseconds / 1000;
  int minutes = seconds / 60;

  seconds = seconds % 60;
  char buffer[6];
  sprintf(buffer, "%d:%02d", minutes, seconds);
  return String(buffer);
}

// void breathLEDs(uint32_t colour, uint8_t maxBrightness, uint8_t cycles,
//   uint8_t stepDelayMs) {
//   // Save current brightness
//   uint8_t original = ledStrip.getBrightness();
//   for (uint8_t c = 0; c < cycles; c++) {
//     for (uint8_t b = 0; b <= maxBrightness; b++) {
//       ledStrip.setBrightness(b);
//       for (int i = 0; i < LED_NUM; i++)
//         ledStrip.setPixelColor(i, colour);
//       ledStrip.show();
//       delay(stepDelayMs);
//     }
//     for (int b = maxBrightness; b >= 0; b--) {
//       ledStrip.setBrightness((uint8_t)b);
//       for (int i = 0; i < LED_NUM; i++)
//         ledStrip.setPixelColor(i, colour);
//       ledStrip.show();
//       delay(stepDelayMs);
//     }
//   }
//   ledStrip.setBrightness(original);
//   ledStrip.show();
// }

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

  // Play/pause button
  display.fillCircle(120, 50, 25, spotify.isPlaying() ? TFT_GREEN : TFT_RED);
  if (spotify.isPlaying()) {
    display.fillRect(108, 38, 8, 24, TFT_BLACK);
    display.fillRect(124, 38, 8, 24, TFT_BLACK);
  }
  else {
    display.fillTriangle(112, 38, 112, 62, 132, 50, TFT_BLACK);
  }

  // Track name
  display.setTextSize(2);
  display.setTextColor(TFT_WHITE);
  display.setCursor(10, 90);
  String track = spotify.getTrackName();
  if (track.length() > 15) track = track.substring(0, 15) + "...";
  display.println(track);


  // Artist name
  display.setTextSize(1);
  display.setTextColor(TFT_LIGHTGREY);
  display.setCursor(10, 110);
  String artist = spotify.getArtistName();
  if (artist.length() > 25) artist = artist.substring(0, 25) + "...";
  display.println(artist);

  // Progress bar
  if (spotify.getTrackDurationMs() > 0) {
    int barWidth = map(spotify.getTrackProgressMs(), 0, spotify.getTrackDurationMs(), 0, 220);
    display.drawRect(10, 135, 220, 8, TFT_WHITE);
    display.fillRect(10, 135, barWidth, 8, TFT_CYAN);

    display.setTextSize(1);
    display.setTextColor(TFT_WHITE);
    display.setCursor(10, 148);
    display.print(formatTime(spotify.getTrackProgressMs()));
    display.setCursor(185, 148);
    display.print(formatTime(spotify.getTrackDurationMs()));
  }

  // Volume bar
  display.setTextSize(1);
  display.setTextColor(TFT_WHITE);
  display.setCursor(10, 175);
  display.print("Volume:");

  int volBarWidth = map(spotify.getVolume(), 0, 100, 0, 160);
  display.drawRect(70, 173, 160, 12, TFT_WHITE);
  display.fillRect(70, 173, volBarWidth, 12, TFT_YELLOW);

  display.setCursor(100, 195);
  display.printf("%d%%", spotify.getVolume());

  // Instructions
  display.setTextSize(1);
  display.setTextColor(TFT_DARKGREY);
  display.setCursor(20, 220);
  display.print("Rotate=Vol | Press=Play");

}

void setup() {
  DebugBegin(115200);
  delay(1000);

  // ledStrip.begin();
  // ledStrip.setBrightness(DEFAULT_LED_BRIGHTNESS);
  // ledStrip.clear();
  // ledStrip.show();

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
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), handleRotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), handleRotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), handleButton, FALLING);


  // Check for Wifi reset (hold encoder button during boot)
  delay(500); // Give time to press button
  if (digitalRead(SWITCH_PIN) == LOW) {
    DebugPrintln("Resetting WiFi credentials...");
    display.fillScreen(TFT_BLACK);
    display.setTextSize(2);
    display.setTextColor(TFT_YELLOW);
    display.setCursor(30, 110);
    display.println("Resetting");
    display.setCursor(50, 130);
    display.println("WiFi...");

    WiFiManager WiFiManager;
    WiFiManager.resetSettings();
    delay(2000);
    ESP.restart();
  }

  connectWiFi();

  // Initialize Spotify
  display.fillScreen(TFT_BLACK);
  display.setCursor(20, 110);
  display.setTextSize(2);
  display.setTextColor(TFT_CYAN);
  display.println("Connecting");
  display.setCursor(40, 130);
  display.println("Spotify...");

  if (!spotify.begin()) {
    DebugPrintln("Spotify init failed!");
    display.fillScreen(TFT_BLACK);
    display.setCursor(30, 110);
    display.setTextColor(TFT_RED);
    display.println("Spotify");
    display.setCursor(50, 130);
    display.println("Failed");
    while (1) delay(1000);
  }

  delay(500);
  spotify.updatePlaybackState();
  updateDisplay();

  DebugPrintln("Setup Complete!");
}

void loop() {
  // Volume control
  if (encoderValue != 0) {
    int newVolume = spotify.getVolume() + (encoderValue * 5);
    encoderValue = 0;

    if (spotify.setVolume(newVolume)) {
      updateDisplay();
    }
  }

  // Play/pause
  if (buttonPressed) {
    buttonPressed = false;
    if (spotify.togglePlayPause()) {
      updateDisplay();
    }
  }

  // Update playback info periodically
  if (millis() - lastPlaybackUpdate > playbackUpdateInterval) {
    lastPlaybackUpdate = millis();
    if (spotify.updatePlaybackState()) {
      updateDisplay();
    }
  }

  delay(50);
}
