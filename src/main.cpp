#include "lgfx_display.h"
// #include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <WiFiManager.h>
#include "credentials.h"
#include "debug.h"
#include "config.h"
#include "spotify_client.h"
#include "display_manager.h"
#include "encoder_controller.h"
#include <Arduino.h>

DisplayManager displayManager;
SpotifyClient spotify(SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, SPOTIFY_REFRESH_TOKEN);
EncoderController encoder(Pins::ENCODER_A, Pins::ENCODER_B, Pins::ENCODER_SWITCH);
// Adafruit_NeoPixel ledStrip = Adafruit_NeoPixel(LED_NUM, LED_PIN, NEO_GRB + NEO_KHZ800);

// Update interval
unsigned long lastPlaybackUpdate = 0;

void initPowerPins() {
  // Power control for the display
  pinMode(Pins::POWER_CONTROL, OUTPUT);
  digitalWrite(Pins::POWER_CONTROL, LOW);

  // Power control for touch controller & I2C
  pinMode(Pins::POWER_ENABLE_1, OUTPUT);
  digitalWrite(Pins::POWER_ENABLE_1, HIGH);
  pinMode(Pins::POWER_ENABLE_2, OUTPUT);
  digitalWrite(Pins::POWER_ENABLE_2, HIGH);
}

bool connectWiFi() {
  DebugPrintln("Starting WiFi configuration...");

  displayManager.showWiFiSetup();


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
  bool connected = wifiManager.autoConnect(Wifi::AP_NAME, Wifi::AP_PASS);

  if (connected) {
    DebugPrintln("\nWiFi Connected!");
    DebugPrint("SSID: ");
    DebugPrintln(WiFi.SSID());
    DebugPrint("IP Address: ");
    DebugPrintln(WiFi.localIP());

    displayManager.showWiFiConnected();
    delay(1000);
    return true;
  }
  else {
    DebugPrintln("\nWiFi connection failed or timeout");
    displayManager.showWiFiFailed();
    delay(3000);
    return false;
  }
}


/**
 * @brief Resets the wifi manager settings when the encoder button is held during boot
 */
void checkWiFiReset() {
  delay(500);
  if (digitalRead(Pins::ENCODER_SWITCH) == LOW) {
    DebugPrintln("WiFi: Resetting credentials...");
    displayManager.showResettingWiFi();

    WiFiManager wifiManager;
    wifiManager.resetSettings();
    delay(2000);
    ESP.restart();
  }
}



void setup() {
  DebugBegin(115200);
  delay(1000);

  // Initialize hardware
  initPowerPins();
  displayManager.begin();
  encoder.begin();

  checkWiFiReset();

  // Connect to Wifi
  if (!connectWiFi()) {
    ESP.restart();
  }

  // Initialize Spotify
  displayManager.showSpotifyConnecting();

  if (!spotify.begin()) {
    DebugPrintln("Spotify init failed!");
    displayManager.showSpotifyFailed();
    while (1) delay(1000);
  }

  // Get initial state
  delay(500);
  spotify.updatePlaybackState();
  displayManager.showPlaybackState(spotify);

  DebugPrintln("Setup Complete!");
}

void loop() {
  // Volume control
  int rotation = encoder.getRotationDelta();
  if (rotation != 0) {
    int newVolume = spotify.getVolume() + (rotation * Encoder::VOLUME_STEP);

    if (spotify.setVolume(newVolume)) {
      displayManager.showPlaybackState(spotify);
    }
  }

  // Play/pause
  if (encoder.wasButtonPressed()) {
    if (spotify.togglePlayPause()) {
      displayManager.showPlaybackState(spotify);
    }
  }

  // Update playback info periodically
  if (millis() - lastPlaybackUpdate > Timing::PLAYBACK_UPDATE_MS) {
    lastPlaybackUpdate = millis();
    if (spotify.updatePlaybackState()) {
      displayManager.showPlaybackState(spotify);
    }
  }

  delay(50);
}
