#pragma once

/**
 * Hardware Pin Definitions
 */
namespace Pins {
  constexpr int ENCODER_A = 45;
  constexpr int ENCODER_B = 42;
  constexpr int ENCODER_SWITCH = 41;
  constexpr int SCREEN_BACKLIGHT = 46;
  constexpr int POWER_CONTROL = 40;
  constexpr int POWER_ENABLE_1 = 1;
  constexpr int POWER_ENABLE_2 = 2;
  constexpr int LED = 48;
}


/**
 * Display Settings
 */
namespace Display {
  constexpr int WIDTH = 240;
  constexpr int HEIGHT = 240;
  constexpr int BACKLIGHT_BRIGHTNESS = 128;
}

/**
 * Wifi Settings
 */
namespace Wifi {
  constexpr const char* AP_NAME = "DeskThing";
  constexpr const char* AP_PASS = "ESP32-DeskThing";
  constexpr int CONFIG_TIMEOUT = 180;
}

/**
 * Update Intervals, Debounce timings, etc.
 */
namespace Timing {
  constexpr unsigned long PLAYBACK_UPDATE_MS = 2000;
  constexpr unsigned long ENCODER_DEBOUNCE_MS = 10;
  constexpr unsigned long BUTTON_DEBOUNCE_MS = 300;
}

/**
 * Rotary Encoder Settings
 */
namespace Encoder {
  constexpr int VOLUME_STEP = 5; // Volume change per click
}

namespace LED {
  constexpr int COUNT = 5;
  constexpr int DEFAULT_BRIGHTNESS = 25;
}