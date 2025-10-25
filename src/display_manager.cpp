#include "display_manager.h"

LGFX display;

DisplayManager::DisplayManager() {}

bool DisplayManager::begin() {
  display.init();
  display.setRotation(0);
  initBacklight();
  DebugPrintln("Display: initialized");
  return true;
}

/**
 * @brief the backlight of the display to 50% brightness
 */
void DisplayManager::initBacklight() {
  ledcSetup(0, 5000, 8);
  ledcAttachPin(Pins::SCREEN_BACKLIGHT, 0);
  ledcWrite(0, Display::BACKLIGHT_BRIGHTNESS);
}

String DisplayManager::formatTime(int milliseconds) {
  int seconds = milliseconds / 1000;
  int minutes = seconds / 60;

  seconds = seconds % 60;
  char buffer[6];
  sprintf(buffer, "%d:%02d", minutes, seconds);
  return String(buffer);
}

void DisplayManager::showWiFiSetup() {
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
  display.println(Wifi::AP_NAME);
  display.setTextSize(1);
  display.setTextColor(TFT_YELLOW);
  display.setCursor(30, 170);
  display.printf("Pass: %s", Wifi::AP_PASS);
}

void DisplayManager::showWiFiConnected() {
  display.fillScreen(TFT_BLACK);
  display.setTextSize(2);
  display.setTextColor(TFT_GREEN);
  display.setCursor(40, 110);
  display.println("WiFi OK!");
}

void DisplayManager::showWiFiFailed() {
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
}

void DisplayManager::showSpotifyConnecting() {
  display.fillScreen(TFT_BLACK);
  display.setCursor(20, 110);
  display.setTextSize(2);
  display.setTextColor(TFT_CYAN);
  display.println("Connecting");
  display.setCursor(40, 130);
  display.println("Spotify...");
}

void DisplayManager::showSpotifyFailed() {
  display.fillScreen(TFT_BLACK);
  display.setCursor(30, 110);
  display.setTextColor(TFT_RED);
  display.setTextSize(2);
  display.println("Spotify");
  display.setCursor(50, 130);
  display.println("Failed");
  display.setTextSize(1);
  display.setCursor(20, 160);
  display.println("Check credentials");
}

void DisplayManager::showResettingWiFi() {
  display.fillScreen(TFT_BLACK);
  display.setTextSize(2);
  display.setTextColor(TFT_YELLOW);
  display.setCursor(30, 110);
  display.println("Resetting");
  display.setCursor(50, 130);
  display.println("WiFi...");
}

void DisplayManager::showPlaybackState(const SpotifyClient& spotify) {
  display.fillScreen(TFT_BLACK);

  // Play/pause button
  display.fillCircle(120, 50, 25, spotify.isPlaying() ? TFT_GREEN : TFT_RED);
  if (spotify.isPlaying()) {
    // Pause icon (two bars)
    display.fillRect(108, 38, 8, 24, TFT_BLACK);
    display.fillRect(124, 38, 8, 24, TFT_BLACK);
  }
  else {
    // Play icon (triangle)
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
  display.setTextColor(TFT_DARKGREY);
  display.setCursor(10, 110);
  String artist = spotify.getArtistName();
  if (artist.length() > 25) artist = artist.substring(0, 25) + "...";
  display.println(artist);

  // Progress bar
  if (spotify.getTrackDurationMs() > 0) {
    int barWidth = map(spotify.getTrackProgressMs(), 0,
      spotify.getTrackDurationMs(), 0, 220);
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

