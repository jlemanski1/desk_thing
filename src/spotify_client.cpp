#include "spotify_client.h"
#include "debug.h"
#include <base64.h>

const char* SpotifyClient::TOKEN_URL = "https://accounts.spotify.com/api/token";
const char* SpotifyClient::PLAYER_URL = "https://api.spotify.com/v1/me/player";
const char* SpotifyClient::PLAY_URL = "https://api.spotify.com/v1/me/player/play";
const char* SpotifyClient::PAUSE_URL = "https://api.spotify.com/v1/me/player/pause";
const char* SpotifyClient::NEXT_URL = "https://api.spotify.com/v1/me/player/next";
const char* SpotifyClient::PREVIOUS_URL = "https://api.spotify.com/v1/me/player/previous";

SpotifyClient::SpotifyClient(const char* clientId, const char* clientSecret, const char* refreshToken)
  : _clientId(clientId)
  , _clientSecret(clientSecret)
  , _refreshToken(refreshToken)
  , _tokenExpiresAt(0)
  , _isPlaying(false)
  , _volume(50)
  , _trackDurationMs(0)
  , _trackProgressMs(0)
{
}

bool SpotifyClient::begin() {
  DebugPrintln("SpotifyClient: Initializing...");
  return refreshAccessToken();
}

bool SpotifyClient::refreshAccessToken() {
  DebugPrintln("SpotifyClient: Refreshing access token...");

  HTTPClient http;
  http.begin(TOKEN_URL);

  // Create authorization header
  String auth = String(_clientId) + ":" + String(_clientSecret);
  String authEncoded = base64::encode(auth);

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Authorization", "Basic " + authEncoded);

  String body = "grant_type=refresh_token&refresh_token=" + String(_refreshToken);

  int httpCode = http.POST(body);

  if (httpCode == 200) {
    String response = http.getString();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
      _accessToken = doc["access_token"].as<String>();
      int expiresIn = doc["expires_in"].as<int>();
      _tokenExpiresAt = millis() + (expiresIn * 1000);

      DebugPrintln("SpotifyClient: Token refreshed");
      http.end();
      return true;
    }
    else {
      DebugPrintln("SpotifyClient: JSON parsing failed");
    }
  }
  else {
    DebugPrintf("SpotifyClient: Token refresh failed: %d\n", httpCode);
    DebugPrintln(http.getString());
  }

  http.end();
  return false;
}

bool SpotifyClient::isTokenValid() const {
  return !_accessToken.isEmpty() && millis() < _tokenExpiresAt;
}

bool SpotifyClient::ensureValidToken() {
  if (!isTokenValid()) {
    return refreshAccessToken();
  }
  return true;
}

bool SpotifyClient::play() {
  if (!ensureValidToken()) return false;

  HTTPClient http;
  http.begin(PLAY_URL);
  http.addHeader("Authorization", "Bearer " + _accessToken);
  http.addHeader("Content-Length", "0");

  int httpCode = http.PUT("");
  bool success = (httpCode == 204 || httpCode == 200);

  if (success) {
    _isPlaying = true;
    DebugPrintln("SpotifyClient: Playing");
  }

  http.end();
  return success;
}

bool SpotifyClient::pause() {
  if (!ensureValidToken()) return false;

  HTTPClient http;
  http.begin(PAUSE_URL);
  http.addHeader("Authorization", "Bearer " + _accessToken);
  http.addHeader("Content-Length", "0");

  int httpCode = http.PUT("");
  bool success = (httpCode == 204 || httpCode == 200);

  if (success) {
    _isPlaying = false;
    DebugPrintln("SpotifyClient: Paused");
  }

  http.end();
  return success;
}

bool SpotifyClient::togglePlayPause() {
  return _isPlaying ? pause() : play();
}

bool SpotifyClient::nextTrack() {
  if (!ensureValidToken()) return false;

  HTTPClient http;
  http.begin(NEXT_URL);
  http.addHeader("Authorization", "Bearer " + _accessToken);
  http.addHeader("Content-Length", "0");

  int httpCode = http.POST("");
  bool success = (httpCode == 204 || httpCode == 200);

  if (success) {
    DebugPrintln("SpotifyClient: Next track");
    // Update state after a short delay
    delay(500);
    updatePlaybackState();
  }

  http.end();
  return success;
}

bool SpotifyClient::previousTrack() {
  if (!ensureValidToken()) return false;

  HTTPClient http;
  http.begin(PREVIOUS_URL);
  http.addHeader("Authorization", "Bearer " + _accessToken);
  http.addHeader("Content-Length", "0");

  int httpCode = http.POST("");
  bool success = (httpCode == 204 || httpCode == 200);

  if (success) {
    DebugPrintln("SpotifyClient: Previous track");
    delay(500);
    updatePlaybackState();
  }

  http.end();
  return success;
}

bool SpotifyClient::setVolume(int volumePercent) {
  if (!ensureValidToken()) return false;

  volumePercent = constrain(volumePercent, 0, 100);

  HTTPClient http;
  String url = String(PLAYER_URL) + "/volume?volume_percent=" + String(volumePercent);

  http.begin(url);
  http.addHeader("Authorization", "Bearer " + _accessToken);

  int httpCode = http.PUT("");
  bool success = (httpCode == 204 || httpCode == 200);

  if (success) {
    _volume = volumePercent;
    DebugPrintf("SpotifyClient: Volume: %d%%\n", volumePercent);
  }

  http.end();
  return success;
}

bool SpotifyClient::seekToPosition(int positionMs) {
  if (!ensureValidToken()) return false;

  HTTPClient http;
  String url = String(PLAYER_URL) + "/seek?position_ms=" + String(positionMs);

  http.begin(url);
  http.addHeader("Authorization", "Bearer " + _accessToken);

  int httpCode = http.PUT("");
  bool success = (httpCode == 204 || httpCode == 200);

  http.end();
  return success;
}

bool SpotifyClient::updatePlaybackState() {
  if (!ensureValidToken()) return false;

  HTTPClient http;
  http.begin(PLAYER_URL);
  http.addHeader("Authorization", "Bearer " + _accessToken);

  int httpCode = http.GET();

  if (httpCode == 200) {
    String response = http.getString();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
      // Playback state
      _isPlaying = doc["is_playing"].as<bool>();
      _trackProgressMs = doc["progress_ms"].as<int>();

      // Volume
      JsonObject device = doc["device"];
      if (!device.isNull()) {
        _volume = device["volume_percent"].as<int>();
      }

      // Track info
      JsonObject item = doc["item"];
      if (!item.isNull()) {
        _trackName = item["name"].as<String>();
        _trackDurationMs = item["duration_ms"].as<int>();

        // Artist(s)
        JsonArray artists = item["artists"];
        if (artists.size() > 0) {
          _artistName = artists[0]["name"].as<String>();
        }

        // Album
        JsonObject album = item["album"];
        if (!album.isNull()) {
          _albumName = album["name"].as<String>();

          // Album art URL (largest image)
          JsonArray images = album["images"];
          if (images.size() > 0) {
            _albumArtUrl = images[0]["url"].as<String>();
          }
        }
      }

      DebugPrintf("SpotifyClient: %s - %s | %s | Vol: %d%%\n",
        _trackName.c_str(), _artistName.c_str(),
        _isPlaying ? "Playing" : "Paused", _volume);

      http.end();
      return true;
    }
    else {
      DebugPrintln("SpotifyClient: JSON parsing failed");
    }
  }
  else if (httpCode == 204) {
    // No active playback
    _trackName = "Nothing Playing";
    _artistName = "";
    _albumName = "";
    _isPlaying = false;
    _trackDurationMs = 0;
    _trackProgressMs = 0;

    DebugPrintln("SpotifyClient: Nothing playing");
    http.end();
    return true;
  }
  else {
    DebugPrintf("SpotifyClient: Get playback failed: %d\n", httpCode);
  }

  http.end();
  return false;
}