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
#include "touch_controller.h"
#include <Arduino.h>
#include <lvgl.h>


static const uint8_t screenWidth = 240;
static const uint8_t screenHeight = 240;
static constexpr uint16_t bufferSize = (screenWidth * 50);

static uint16_t lv_buffer[bufferSize];

DisplayManager displayManager;
SpotifyClient spotify(SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, SPOTIFY_REFRESH_TOKEN);
EncoderController encoder(Pins::ENCODER_A, Pins::ENCODER_B, Pins::ENCODER_SWITCH);
TouchController touch(Pins::TOUCH_SDA, Pins::TOUCH_SCL, Pins::TOUCH_RST, Pins::TOUCH_INT);
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



static void onButtonClick(lv_event_t* e) {
  lv_event_code_t eventCode = lv_event_get_code(e);
  lv_obj_t* button = lv_event_get_target_obj(e);

  if (eventCode == LV_EVENT_CLICKED) {
    static uint8_t count = 0;
    count++;

    // Get the first child of the button (label) and update its text
    lv_obj_t* label = lv_obj_get_child(button, 0);
    lv_label_set_text_fmt(label, "Button: %d", count);
  }
}

static void encoderValueChanged(lv_event_t* e) {
  lv_obj_t* arc = lv_event_get_target_obj(e);
  lv_obj_t* label = (lv_obj_t*)lv_event_get_user_data(e);

  lv_label_set_text_fmt(label, "%" LV_PRId32 "%%", lv_arc_get_value(arc));
}

uint32_t msCallback() {
  return millis();
}


void setup() {
  DebugBegin(115200);
  delay(1000);

  // Initialize hardware
  initPowerPins();
  displayManager.begin();
  encoder.begin();
  Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
  touch.begin();

  // Init LVGL
  lv_init();
  lv_tick_set_cb(msCallback);

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
  lv_display_set_user_data(lvglDisplay, &display);

  // Set flush callback
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

  // Set up touch controller as LVGL input device
  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_user_data(indev, &touch);

  lv_indev_set_read_cb(indev, [](lv_indev_t* indev_drv, lv_indev_data_t* data) {
    TouchController* touch_ctrl = (TouchController*)lv_indev_get_user_data(indev_drv);
    uint16_t touchX = 0, touchY = 0;
    uint8_t gesture = 0;

    bool touched = touch_ctrl->getTouch(&touchX, &touchY, &gesture);
    Serial.println('TOUCHED!');
    if (touched) {
      data->state = LV_INDEV_STATE_PRESSED;
      data->point.x = touchX;
      data->point.y = touchY;

      static uint16_t lastX = 0, lastY = 0;
      if (touchX != lastX || touchY != lastY) {
        DebugPrint("Touch: ");
        DebugPrint(touchX);
        DebugPrint(", ");
        DebugPrintln(touchY);
        lastX = touchX;
        lastY = touchY;
      }
    }
    else {
      data->state = LV_INDEV_STATE_RELEASED;
    }
    });
  // Set to event-driven mode
  lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);

  // TODO: Hook up Touch Controller touch handling to LVGL (encoder done)



  // Set up Encoder as LVGL input device for volume control
  lv_indev_t* encoder_indev = lv_indev_create();
  lv_indev_set_type(encoder_indev, LV_INDEV_TYPE_ENCODER);
  lv_indev_set_user_data(encoder_indev, &encoder);

  lv_indev_set_read_cb(encoder_indev, [](lv_indev_t* indev_drv, lv_indev_data_t* data) {
    EncoderController* enc = (EncoderController*)lv_indev_get_user_data(indev_drv);

    uint32_t tick_now = lv_tick_get();

    // If no interrupt has happened in the past 100 ms, pause the indev timer
    if (lv_tick_elaps(enc->getLastInterruptTick()) > 100) {
      lv_timer_t* timer = lv_indev_get_read_timer(indev_drv);
      lv_timer_pause(timer);
    }

    if (enc->checkAndClearInterruptFlag()) {
      enc->updateLastInterruptTick();

      // Ensure the timer is running in case an interrupt occurred
      // just after the timer was paused (race condition prevention)
      lv_timer_t* timer = lv_indev_get_read_timer(indev_drv);
      lv_timer_resume(timer);
    }
    // Get rotation delta
    int rotation = enc->getRotationDelta();
    data->enc_diff = rotation;

    // Get button state
    data->state = enc->isButtonPressed() ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    });

  // Get the timer and store it in the encoder controller
  lv_timer_t* encoder_timer = lv_indev_get_read_timer(encoder_indev);
  encoder.setLvglIndevTimer(encoder_timer);
  // Set encoder to event driven mode
  lv_indev_set_mode(encoder_indev, LV_INDEV_MODE_EVENT);

  // Create a group for the encoder
  lv_group_t* encoder_group = lv_group_create();
  lv_indev_set_group(encoder_indev, encoder_group);

  // Associate the encoder input device with the group
  lv_indev_set_group(encoder_indev, encoder_group);



  // Set up UI
  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);

  // Create label
  lv_obj_t* label = lv_label_create(screen);
  lv_label_set_text(label, "Hello\nWorld!");
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, -30);

  // Create a button with click event
  lv_obj_t* button = lv_button_create(screen);
  lv_obj_set_size(button, 120, 50);
  lv_obj_align(button, LV_ALIGN_CENTER, 0, 40);
  lv_obj_add_event_cb(button, onButtonClick, LV_EVENT_ALL, NULL);

  // lv_obj_add_event_cb(button, [](lv_event_t* e) {
  //   lv_obj_t* button = (lv_obj_t*)lv_event_get_target(e);
  //   static bool is_green = false;

  //   if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
  //     is_green = !is_green;

  //     if (is_green) {
  //       // Change to green
  //       lv_obj_set_style_bg_color(button, lv_color_hex(0x00FF00), 0);
  //       DebugPrintln("Button: GREEN");
  //     }
  //     else {
  //       // Change to blue
  //       lv_obj_set_style_bg_color(button, lv_color_hex(0x2196F3), 0);
  //       DebugPrintln("Button: BLUE");
  //     }
  //   }
  //   }, LV_EVENT_CLICKED, NULL);

  // Create button label
  lv_obj_t* buttonLabel = lv_label_create(button);
  lv_label_set_text(buttonLabel, "Click Me");
  lv_obj_center(buttonLabel);


  // Create a group for the encoder

  // Create Volume Arc
  lv_obj_t* volumeLabel = lv_label_create(screen);
  lv_obj_t* volumeArc = lv_arc_create(screen);
  lv_obj_set_size(volumeArc, 220, 220);
  lv_arc_set_rotation(volumeArc, 135);
  lv_arc_set_bg_angles(volumeArc, 0, 270);
  lv_arc_set_value(volumeArc, 10);
  lv_obj_center(volumeArc);
  lv_obj_add_event_cb(volumeArc, encoderValueChanged, LV_EVENT_VALUE_CHANGED, volumeLabel);

  // Position the label inside the arc
  lv_obj_align_to(volumeLabel, volumeArc, LV_ALIGN_CENTER, 0, 0);

  // Add the volumeArc to the group so it can receive encoder input
  lv_group_add_obj(encoder_group, volumeArc);


  /*Manually update the label for the first time*/
  lv_obj_send_event(volumeArc, LV_EVENT_VALUE_CHANGED, NULL);

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
