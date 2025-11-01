#pragma once

#include <ArduinoJson.h>
#include "lgfx_display.h"
#include "spotify_client.h"
#include "config.h"
#include "debug.h"

class SpotifyClient;


/**
 * @brief High level display manager
 */
class DisplayManager {
public:
  DisplayManager();

  bool begin();

  // UI screens
  void showWiFiSetup();
  void showWiFiConnected();
  void showWiFiFailed();
  void showSpotifyConnecting();
  void showSpotifyFailed();
  void showResettingWiFi();
  void showPlaybackState(const SpotifyClient& spotify);


private:
  void initBacklight();
  String formatTime(int milliseconds);
};