#define LGFX_USE_V1

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
#include <lvgl.h>


static const uint8_t screenWidth = 240;
static const uint8_t screenHeight = 240;
static constexpr uint16_t bufferSize = (screenWidth * 50);

static uint16_t lv_buffer[bufferSize];

/*
 * LVGL tick callback
 * Returns the ms elapsed since startup
*/
static uint32_t getMillis(void) {
  return millis();
}



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
  // encoder.begin();


  // Init LVGL
  lv_init();
  // lv_tick_set_cb(getMillis);


  // Create Display Buffer
  lv_display_t* lvglDisplay = lv_display_create(screenWidth, screenHeight);
  // Allocate draw buffers
  size_t buf_size = screenWidth * 50; // 50 lines
  void* buf1 = heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!buf1) {
    DebugPrintln("ERROR: Failed to allocate LVGL buffer!");
    while (1) delay(1000);
  }

  lv_display_set_buffers(lvglDisplay, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

  // Set user data and flush callback
  lv_display_set_user_data(lvglDisplay, &display);

  lv_display_set_flush_cb(lvglDisplay, [](lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    LGFX* gfx = (LGFX*)lv_display_get_user_data(disp);
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    gfx->startWrite();
    gfx->setAddrWindow(area->x1, area->y1, w, h);
    gfx->writePixels((lgfx::rgb565_t*)px_map, w * h);
    gfx->endWrite();

    lv_display_flush_ready(disp);
    });


  // Test UI
  // Create UI
  lv_obj_t* scr = lv_screen_active();

  // Set background color
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

  // Create label
  lv_obj_t* label = lv_label_create(scr);
  lv_label_set_text(label, "Hello\nWorld!");
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, -30);

  // Create a button
  lv_obj_t* btn = lv_button_create(scr);
  lv_obj_set_size(btn, 120, 50);
  lv_obj_align(btn, LV_ALIGN_CENTER, 0, 40);

  lv_obj_t* btn_label = lv_label_create(btn);
  lv_label_set_text(btn_label, "Click Me");
  lv_obj_center(btn_label);

  // Force initial render
  lv_refr_now(lvglDisplay);

  // checkWiFiReset();

  // // Connect to Wifi
  // if (!connectWiFi()) {
  //   ESP.restart();
  // }

  // // Initialize Spotify
  // displayManager.showSpotifyConnecting();

  // if (!spotify.begin()) {
  //   DebugPrintln("Spotify init failed!");
  //   displayManager.showSpotifyFailed();
  //   while (1) delay(1000);
  // }

  // // Get initial state
  // delay(500);
  // spotify.updatePlaybackState();
  // displayManager.showPlaybackState(spotify);

  DebugPrintln("Setup Complete!");
}

void loop() {
  lv_timer_handler(); // Let LVGL handle rendering and events
  delay(5);
  // // Volume control
  // int rotation = encoder.getRotationDelta();
  // if (rotation != 0) {
  //   int newVolume = spotify.getVolume() + (rotation * Encoder::VOLUME_STEP);

  //   if (spotify.setVolume(newVolume)) {
  //     displayManager.showPlaybackState(spotify);
  //   }
  // }

  // // Play/pause
  // if (encoder.wasButtonPressed()) {
  //   if (spotify.togglePlayPause()) {
  //     displayManager.showPlaybackState(spotify);
  //   }
  // }

  // // Update playback info periodically
  // if (millis() - lastPlaybackUpdate > Timing::PLAYBACK_UPDATE_MS) {
  //   lastPlaybackUpdate = millis();
  //   if (spotify.updatePlaybackState()) {
  //     displayManager.showPlaybackState(spotify);
  //   }
  // }

  // delay(50);
}
