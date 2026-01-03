#define LGFX_USE_V1

#include "lgfx_display.h"
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
#include <math.h>

static const uint8_t screenWidth = 240;
static const uint8_t screenHeight = 240;
static constexpr uint16_t bufferSize = (screenWidth * 50);

static uint16_t lv_buffer[bufferSize];

DisplayManager displayManager;
SpotifyClient spotify(SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, SPOTIFY_REFRESH_TOKEN);
EncoderController encoder(Pins::ENCODER_A, Pins::ENCODER_B, Pins::ENCODER_SWITCH);
TouchController touch(Pins::TOUCH_SDA, Pins::TOUCH_SCL, Pins::TOUCH_RST, Pins::TOUCH_INT);

// Update interval
unsigned long lastPlaybackUpdate = 0;
const uint32_t SPOTIFY_UPDATE_INTERVAL = 2000;

// UI Mode
typedef enum {
  MODE_VOLUME,
  MODE_SEEK
} ui_mode_t;

static ui_mode_t g_mode = MODE_VOLUME;

// UI Objects
static lv_obj_t* volume_arc = NULL;
static lv_obj_t* progress_arc = NULL;
static lv_obj_t* volume_label = NULL;
static lv_obj_t* time_label = NULL;
static lv_obj_t* track_label = NULL;
static lv_obj_t* artist_label = NULL;
static lv_obj_t* play_btn = NULL;
static lv_obj_t* play_label = NULL;
static lv_obj_t* orbit_dot = NULL;
static lv_obj_t* mode_label = NULL;
static lv_timer_t* orbit_timer = NULL;
static lv_timer_t* spotify_update_timer = NULL;

// Forward declarations
static void play_btn_event_cb(lv_event_t* e);
static void volume_arc_event_cb(lv_event_t* e);
static void screen_gesture_cb(lv_event_t* e);
static void orbit_timer_cb(lv_timer_t* timer);
static void spotify_update_timer_cb(lv_timer_t* timer);
static void format_time(char* buf, uint32_t ms);
static void update_ui_from_spotify();

void initPowerPins() {
  pinMode(Pins::POWER_CONTROL, OUTPUT);
  digitalWrite(Pins::POWER_CONTROL, LOW);

  pinMode(Pins::POWER_ENABLE_1, OUTPUT);
  digitalWrite(Pins::POWER_ENABLE_1, HIGH);
  pinMode(Pins::POWER_ENABLE_2, OUTPUT);
  digitalWrite(Pins::POWER_ENABLE_2, HIGH);
}

bool connectWiFi() {
  DebugPrintln("Starting WiFi configuration...");

  displayManager.showWiFiSetup();

  WiFiManager wifiManager;
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

uint32_t msCallback() {
  return millis();
}

// Create the circular music player UI
void createMusicPlayerUI(lv_obj_t* parent) {
  // Set dark background
  lv_obj_set_style_bg_color(parent, lv_color_hex(0x0f172a), 0);

  // Create main container
  lv_obj_t* main_cont = lv_obj_create(parent);
  lv_obj_set_size(main_cont, screenWidth, screenHeight);
  lv_obj_center(main_cont);
  lv_obj_set_style_bg_color(main_cont, lv_color_hex(0x0f172a), 0);
  lv_obj_set_style_border_width(main_cont, 0, 0);
  lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);

  // Outer glow circle
  lv_obj_t* glow = lv_obj_create(main_cont);
  lv_obj_set_size(glow, 220, 220);
  lv_obj_center(glow);
  lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(glow, lv_color_hex(0x4f46e5), 0);
  lv_obj_set_style_bg_opa(glow, LV_OPA_20, 0);
  lv_obj_set_style_border_width(glow, 0, 0);

  // Volume arc (top half)
  volume_arc = lv_arc_create(main_cont);
  lv_obj_set_size(volume_arc, 200, 200);
  lv_obj_center(volume_arc);
  lv_arc_set_bg_angles(volume_arc, 180, 360);
  lv_arc_set_range(volume_arc, 0, 100);
  lv_arc_set_value(volume_arc, 50);
  lv_obj_set_style_arc_width(volume_arc, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(volume_arc, 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(volume_arc, lv_color_hex(0x4f46e5), LV_PART_MAIN);
  lv_obj_set_style_arc_opa(volume_arc, LV_OPA_30, LV_PART_MAIN);
  lv_obj_set_style_arc_color(volume_arc, lv_color_hex(0x8b5cf6), LV_PART_INDICATOR);
  lv_obj_remove_style(volume_arc, NULL, LV_PART_KNOB);
  lv_obj_add_event_cb(volume_arc, volume_arc_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  // Progress arc (bottom orbit rings)
  progress_arc = lv_arc_create(main_cont);
  lv_obj_set_size(progress_arc, 180, 180);
  lv_obj_center(progress_arc);
  lv_arc_set_bg_angles(progress_arc, 0, 360);
  lv_arc_set_range(progress_arc, 0, 100);
  lv_arc_set_value(progress_arc, 0);
  lv_obj_set_style_arc_width(progress_arc, 2, LV_PART_MAIN);
  lv_obj_set_style_arc_width(progress_arc, 2, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x4f46e5), LV_PART_MAIN);
  lv_obj_set_style_arc_opa(progress_arc, LV_OPA_30, LV_PART_MAIN);
  lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0xa855f7), LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(progress_arc, LV_OPA_50, LV_PART_INDICATOR);
  lv_obj_remove_style(progress_arc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(progress_arc, LV_OBJ_FLAG_CLICKABLE);

  // Orbiting dot
  orbit_dot = lv_obj_create(main_cont);
  lv_obj_set_size(orbit_dot, 8, 8);
  lv_obj_set_style_radius(orbit_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(orbit_dot, lv_color_hex(0xa855f7), 0);
  lv_obj_set_style_border_width(orbit_dot, 0, 0);
  orbit_timer = lv_timer_create(orbit_timer_cb, 50, NULL);

  // Center planet circle
  lv_obj_t* planet = lv_obj_create(main_cont);
  lv_obj_set_size(planet, 140, 140);
  lv_obj_center(planet);
  lv_obj_set_style_radius(planet, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(planet, lv_color_hex(0x4f46e5), 0);
  lv_obj_set_style_bg_grad_color(planet, lv_color_hex(0x7c3aed), 0);
  lv_obj_set_style_bg_grad_dir(planet, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(planet, 0, 0);
  lv_obj_set_style_shadow_width(planet, 30, 0);
  lv_obj_set_style_shadow_color(planet, lv_color_hex(0x4f46e5), 0);
  lv_obj_set_style_shadow_opa(planet, LV_OPA_50, 0);

  // Play button
  play_btn = lv_button_create(planet);
  lv_obj_set_size(play_btn, 60, 60);
  lv_obj_align(play_btn, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_radius(play_btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(play_btn, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_opa(play_btn, LV_OPA_10, 0);
  lv_obj_add_event_cb(play_btn, play_btn_event_cb, LV_EVENT_CLICKED, NULL);

  play_label = lv_label_create(play_btn);
  lv_label_set_text(play_label, LV_SYMBOL_PLAY);
  lv_obj_center(play_label);

  // Track name
  track_label = lv_label_create(planet);
  lv_label_set_text(track_label, "No Track");
  lv_label_set_long_mode(track_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(track_label, 120);
  lv_obj_align(track_label, LV_ALIGN_CENTER, 0, 25);
  lv_obj_set_style_text_color(track_label, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_align(track_label, LV_TEXT_ALIGN_CENTER, 0);

  // Artist name
  artist_label = lv_label_create(planet);
  lv_label_set_text(artist_label, "No Artist");
  lv_label_set_long_mode(artist_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(artist_label, 120);
  lv_obj_align(artist_label, LV_ALIGN_CENTER, 0, 43);
  lv_obj_set_style_text_color(artist_label, lv_color_hex(0xc7d2fe), 0);
  lv_obj_set_style_text_align(artist_label, LV_TEXT_ALIGN_CENTER, 0);

  // Time label
  time_label = lv_label_create(planet);
  lv_label_set_text(time_label, "0:00 / 0:00");
  lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 58);
  lv_obj_set_style_text_color(time_label, lv_color_hex(0xa5b4fc), 0);

  // Volume label
  volume_label = lv_label_create(main_cont);
  lv_label_set_text_fmt(volume_label, LV_SYMBOL_VOLUME_MAX " 50%%");
  lv_obj_align(volume_label, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_set_style_text_color(volume_label, lv_color_hex(0xa5b4fc), 0);

  // Mode indicator label (bottom)
  mode_label = lv_label_create(main_cont);
  lv_label_set_text(mode_label, "VOLUME");
  lv_obj_align(mode_label, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_text_color(mode_label, lv_color_hex(0x6366f1), 0);

  // Add gesture handling
  lv_obj_add_event_cb(main_cont, screen_gesture_cb, LV_EVENT_GESTURE, NULL);
  lv_obj_add_flag(main_cont, LV_OBJ_FLAG_GESTURE_BUBBLE);

  // Create Spotify update timer
  spotify_update_timer = lv_timer_create(spotify_update_timer_cb, SPOTIFY_UPDATE_INTERVAL, NULL);
}

// Update UI from Spotify state
static void update_ui_from_spotify() {
  // Update volume arc and label
  int volume = spotify.getVolume();
  lv_arc_set_value(volume_arc, volume);
  lv_label_set_text_fmt(volume_label, LV_SYMBOL_VOLUME_MAX " %d%%", volume);

  // Update progress arc
  int duration = spotify.getTrackDurationMs();
  int progress = spotify.getTrackProgressMs();

  if (duration > 0) {
    int progress_percent = (progress * 100) / duration;
    lv_arc_set_value(progress_arc, progress_percent);
  }
  else {
    lv_arc_set_value(progress_arc, 0);
  }

  // Update time label
  char time_buf[32];
  format_time(time_buf, progress);
  strcat(time_buf, " / ");
  char duration_buf[16];
  format_time(duration_buf, duration);
  strcat(time_buf, duration_buf);
  lv_label_set_text(time_label, time_buf);

  // Update play/pause icon
  const char* icon = spotify.isPlaying() ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
  lv_label_set_text(play_label, icon);

  // Update track info
  String trackName = spotify.getTrackName();
  String artistName = spotify.getArtistName();

  if (trackName.length() > 0) {
    lv_label_set_text(track_label, trackName.c_str());
  }
  else {
    lv_label_set_text(track_label, "No Track");
  }

  if (artistName.length() > 0) {
    lv_label_set_text(artist_label, artistName.c_str());
  }
  else {
    lv_label_set_text(artist_label, "");
  }
}

// Timer callback for Spotify updates
static void spotify_update_timer_cb(lv_timer_t* timer) {
  spotify.updatePlaybackState();
  update_ui_from_spotify();
}

// Event callbacks
static void play_btn_event_cb(lv_event_t* e) {
  spotify.togglePlayPause();
  delay(300);
  spotify.updatePlaybackState();
  update_ui_from_spotify();
}

static void volume_arc_event_cb(lv_event_t* e) {
  lv_obj_t* arc = lv_event_get_target_obj(e);
  int volume = lv_arc_get_value(arc);
  lv_label_set_text_fmt(volume_label, LV_SYMBOL_VOLUME_MAX " %d%%", volume);
  spotify.setVolume(volume);
}

static void screen_gesture_cb(lv_event_t* e) {
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

  if (dir == LV_DIR_LEFT) {
    spotify.nextTrack();
  }
  else if (dir == LV_DIR_RIGHT) {
    spotify.previousTrack();
  }
}

static void orbit_timer_cb(lv_timer_t* timer) {
  static int angle = 0;

  // Only animate if playing
  if (spotify.isPlaying()) {
    angle = (angle + 2) % 360;
  }

  // Calculate orbit position
  int center_x = screenWidth / 2;
  int center_y = screenHeight / 2;
  int radius = 90;

  float rad = angle * 3.14159 / 180.0;
  int x = center_x + (int)(radius * cos(rad)) - 4;
  int y = center_y + (int)(radius * sin(rad)) - 4;

  lv_obj_set_pos(orbit_dot, x, y);
}

static void format_time(char* buf, uint32_t ms) {
  uint32_t seconds = ms / 1000;
  uint32_t mins = seconds / 60;
  uint32_t secs = seconds % 60;
  snprintf(buf, 16, "%d:%02d", mins, secs);
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
  size_t buf_size = screenWidth * 50;
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

    if (touched) {
      data->state = LV_INDEV_STATE_PRESSED;
      data->point.x = touchX;
      data->point.y = touchY;
    }
    else {
      data->state = LV_INDEV_STATE_RELEASED;
    }
    });

  lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);

  // Set up Encoder as LVGL input device for volume control
  lv_indev_t* encoder_indev = lv_indev_create();
  lv_indev_set_type(encoder_indev, LV_INDEV_TYPE_ENCODER);
  lv_indev_set_user_data(encoder_indev, &encoder);

  lv_indev_set_read_cb(encoder_indev, [](lv_indev_t* indev_drv, lv_indev_data_t* data) {
    EncoderController* enc = (EncoderController*)lv_indev_get_user_data(indev_drv);

    // Pause timer if no recent activity
    if (lv_tick_elaps(enc->getLastInterruptTick()) > 100) {
      lv_timer_t* timer = lv_indev_get_read_timer(indev_drv);
      lv_timer_pause(timer);
    }

    if (enc->checkAndClearInterruptFlag()) {
      enc->updateLastInterruptTick();
      lv_timer_t* timer = lv_indev_get_read_timer(indev_drv);
      lv_timer_resume(timer);
    }

    // Handle encoder rotation
    int rotation = enc->getRotationDelta();
    if (rotation != 0) {
      if (g_mode == MODE_VOLUME) {
        int current_vol = spotify.getVolume();
        int new_vol = current_vol + rotation;
        new_vol = constrain(new_vol, 0, 100);
        lv_arc_set_value(volume_arc, new_vol);
        lv_label_set_text_fmt(volume_label, LV_SYMBOL_VOLUME_MAX " %d%%", new_vol);
        spotify.setVolume(new_vol);
      }
      else if (g_mode == MODE_SEEK) {
        int current_pos = spotify.getTrackProgressMs();
        int duration = spotify.getTrackDurationMs();
        int new_pos = current_pos + (rotation * 5000); // 5 seconds
        new_pos = constrain(new_pos, 0, duration);
        spotify.seekToPosition(new_pos);
        delay(100);
        spotify.updatePlaybackState();
        update_ui_from_spotify();
      }
    }

    // Handle button press
    static bool last_btn_state = false;
    static uint32_t press_start = 0;
    bool btn_pressed = enc->isButtonPressed();

    if (btn_pressed && !last_btn_state) {
      press_start = millis();
    }
    else if (!btn_pressed && last_btn_state) {
      uint32_t press_duration = millis() - press_start;
      if (press_duration >= 1000) {
        // Long press - toggle mode
        g_mode = (g_mode == MODE_VOLUME) ? MODE_SEEK : MODE_VOLUME;
        lv_label_set_text(mode_label, g_mode == MODE_VOLUME ? "VOLUME" : "SEEK");
        lv_obj_set_style_text_color(mode_label, g_mode == MODE_VOLUME ? lv_color_hex(0x6366f1) : lv_color_hex(0xa855f7), 0);
      }
      else {
        // Short press - play/pause
        spotify.togglePlayPause();
        delay(300);
        spotify.updatePlaybackState();
        update_ui_from_spotify();
      }
    }
    last_btn_state = btn_pressed;

    data->state = LV_INDEV_STATE_RELEASED;
    });

  lv_timer_t* encoder_timer = lv_indev_get_read_timer(encoder_indev);
  encoder.setLvglIndevTimer(encoder_timer);
  lv_indev_set_mode(encoder_indev, LV_INDEV_MODE_EVENT);

  // Create a group for the encoder
  lv_group_t* encoder_group = lv_group_create();
  lv_indev_set_group(encoder_indev, encoder_group);

  // Set up UI
  lv_obj_t* screen = lv_screen_active();

  // Create the circular music player UI
  createMusicPlayerUI(screen);

  // Add volume arc to encoder group
  lv_group_add_obj(encoder_group, volume_arc);

  // Force initial render
  lv_refr_now(lvglDisplay);

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
  update_ui_from_spotify();

  DebugPrintln("Setup Complete!");
}

void loop() {
  lv_timer_handler();
  delay(5);
}